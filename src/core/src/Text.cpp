// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Text.h"

#include "forge/core/Document.h"  // GeometryError

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Font_FontMgr.hxx>
#include <Font_SystemFont.hxx>
#include <NCollection_String.hxx>
#include <Poly_Triangulation.hxx>
#include <StdPrs_BRepFont.hxx>
#include <StdPrs_BRepTextBuilder.hxx>
#include <TCollection_AsciiString.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <mutex>
#include <numbers>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace forge::text {

namespace {

// OpenCASCADE's font manager and FreeType are not thread-safe: one at a time.
std::mutex& fontMutex()
{
    static std::mutex m;
    return m;
}

std::string lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Needs fontMutex held.
const std::vector<std::string>& fontsLocked()
{
    static const std::vector<std::string> list = [] {
        std::vector<std::string> out;
        const auto manager = Font_FontMgr::GetInstance();
        for (const auto& font : manager->GetAvailableFonts())
            out.emplace_back(font->FontName().ToCString());
        std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
            const std::string la = lower(a), lb = lower(b);
            return la != lb ? la < lb : a < b;
        });
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }();
    return list;
}

std::string defaultLocked()
{
    const auto& fonts = fontsLocked();
    for (const char* preferred : {"Arial", "Segoe UI", "Liberation Sans", "DejaVu Sans", "FreeSans", "Noto Sans"})
        for (const std::string& f : fonts)
            if (lower(f) == lower(preferred))
                return f;
    for (const std::string& f : fonts)
        if (lower(f).find("sans") != std::string::npos)
            return f;
    return fonts.empty() ? std::string("Arial") : fonts.front();
}

std::string resolvedLocked(const std::string& name)
{
    if (!name.empty())
        for (const std::string& f : fontsLocked())
            if (lower(f) == lower(name))
                return f;
    return defaultLocked();
}

Font_FontAspect aspectOf(bool bold, bool italic)
{
    if (bold)
        return italic ? Font_FA_BoldItalic : Font_FA_Bold;
    return italic ? Font_FA_Italic : Font_FA_Regular;
}

struct Box2 {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double width() const { return x1 - x0; }
    double height() const { return y1 - y0; }
};

Box2 boundsOf(const TopoDS_Shape& shape)
{
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, /*useTriangulation=*/false, /*useShapeTolerance=*/false);
    double z0 = 0, z1 = 0;
    Box2 b;
    box.Get(b.x0, b.y0, z0, b.x1, b.y1, z1);
    return b;
}

// Natural size of the words at font size 10 mm (cached: the same text is laid out often).
Box2 naturalBox(const std::string& text, const std::string& font, bool bold, bool italic)
{
    static std::mutex cacheMutex;
    static std::map<std::tuple<std::string, std::string, bool, bool>, Box2> cache;
    const auto key = std::make_tuple(text, resolvedFont(font), bold, italic);
    {
        std::lock_guard lock(cacheMutex);
        if (const auto it = cache.find(key); it != cache.end())
            return it->second;
    }
    const Box2 b = boundsOf(letterFaces(text, font, bold, italic, 10.0));
    std::lock_guard lock(cacheMutex);
    if (cache.size() > 2000)
        cache.clear();
    cache[key] = b;
    return b;
}

// Splits every triangle edge longer than maxEdge at its middle (and the triangles
// on both sides of it) until none is. Shared middles keep the triangles joined.
void refine(std::vector<std::array<double, 2>>& pts, std::vector<std::uint32_t>& tris, double maxEdge)
{
    const double limit2 = maxEdge * maxEdge;
    const auto len2 = [&pts](std::uint32_t a, std::uint32_t b) {
        const double dx = pts[a][0] - pts[b][0], dy = pts[a][1] - pts[b][1];
        return dx * dx + dy * dy;
    };
    for (int pass = 0; pass < 24; ++pass) {
        std::unordered_map<std::uint64_t, std::uint32_t> middles;
        const auto key = [](std::uint32_t a, std::uint32_t b) {
            return a < b ? (std::uint64_t(a) << 32 | b) : (std::uint64_t(b) << 32 | a);
        };
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3)
            for (int e = 0; e < 3; ++e) {
                const std::uint32_t a = tris[t + e], b = tris[t + (e + 1) % 3];
                if (len2(a, b) > limit2 && !middles.count(key(a, b))) {
                    middles[key(a, b)] = static_cast<std::uint32_t>(pts.size());
                    pts.push_back({(pts[a][0] + pts[b][0]) / 2, (pts[a][1] + pts[b][1]) / 2});
                }
            }
        if (middles.empty())
            return;
        if (tris.size() > 6'000'000)
            throw GeometryError("The letters are too big for this surface detail. Make them smaller.");
        std::vector<std::uint32_t> next;
        next.reserve(tris.size() * 3);
        const auto emit = [&next](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
            next.insert(next.end(), {a, b, c});
        };
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
            std::array<std::uint32_t, 3> v{tris[t], tris[t + 1], tris[t + 2]};
            std::array<std::int64_t, 3> m{};
            int count = 0;
            for (int e = 0; e < 3; ++e) {
                const auto it = middles.find(key(v[e], v[(e + 1) % 3]));
                m[e] = it == middles.end() ? std::int64_t(-1) : std::int64_t(it->second);
                count += it == middles.end() ? 0 : 1;
            }
            // Turn the triangle so the pattern starts at corner 0.
            const auto rotate = [&v, &m] {
                std::rotate(v.begin(), v.begin() + 1, v.end());
                std::rotate(m.begin(), m.begin() + 1, m.end());
            };
            if (count == 0) {
                emit(v[0], v[1], v[2]);
            } else if (count == 3) {
                const auto ab = std::uint32_t(m[0]), bc = std::uint32_t(m[1]), ca = std::uint32_t(m[2]);
                emit(v[0], ab, ca);
                emit(ab, v[1], bc);
                emit(ca, bc, v[2]);
                emit(ab, bc, ca);
            } else if (count == 1) {
                while (m[0] < 0)
                    rotate();
                const auto ab = std::uint32_t(m[0]);
                emit(v[0], ab, v[2]);
                emit(ab, v[1], v[2]);
            } else {
                while (m[2] >= 0)  // the edge from corner 2 back to 0 is the one kept
                    rotate();
                const auto ab = std::uint32_t(m[0]), bc = std::uint32_t(m[1]);
                emit(ab, v[1], bc);
                if (len2(v[0], bc) < len2(ab, v[2])) {
                    emit(v[0], ab, bc);
                    emit(v[0], bc, v[2]);
                } else {
                    emit(v[0], ab, v[2]);
                    emit(ab, bc, v[2]);
                }
            }
        }
        tris = std::move(next);
    }
}

}  // namespace

std::vector<std::string> availableFonts()
{
    std::lock_guard lock(fontMutex());
    return fontsLocked();
}

void warmUp()
{
    std::lock_guard lock(fontMutex());
    (void)fontsLocked();
}

std::string defaultFont()
{
    std::lock_guard lock(fontMutex());
    return defaultLocked();
}

std::string resolvedFont(const std::string& name)
{
    std::lock_guard lock(fontMutex());
    return resolvedLocked(name);
}

TopoDS_Shape letterFaces(const std::string& text, const std::string& font, bool bold, bool italic, double emSize)
{
    if (!(emSize > 0.0))
        throw std::invalid_argument("The letters must be bigger than zero");
    std::lock_guard lock(fontMutex());
    const std::string name = resolvedLocked(font);
    StdPrs_BRepFont brepFont;
    if (!brepFont.FindAndInit(TCollection_AsciiString(name.c_str()), aspectOf(bold, italic), emSize,
                              Font_StrictLevel_Any))
        throw GeometryError("The font '" + name + "' could not be loaded");
    StdPrs_BRepTextBuilder builder;
    const TopoDS_Shape faces = builder.Perform(brepFont, NCollection_String(text.c_str()));
    if (faces.IsNull() || !TopExp_Explorer(faces, TopAbs_FACE).More())
        throw std::invalid_argument("Type some text to make letters");
    return faces;
}

double capitalRatio(const std::string& font, bool bold, bool italic)
{
    const Box2 h = naturalBox("H", font, bold, italic);
    return h.height() > 0.0 ? h.height() / 10.0 : 0.7;
}

TopoDS_Shape makeText(const TextSpec& spec)
{
    if (!(spec.width > 0.0) || !(spec.depth > 0.0) || !(spec.height > 0.0))
        throw std::invalid_argument("The text's width, depth and thickness must be greater than zero");
    const TopoDS_Shape faces = letterFaces(spec.text, spec.font, spec.bold, spec.italic, 10.0);
    const Box2 b = boundsOf(faces);
    if (!(b.width() > 0.0) || !(b.height() > 0.0))
        throw std::invalid_argument("Type some text to make letters");
    const double sx = spec.width / b.width(), sy = spec.depth / b.height();
    gp_Trsf centre;
    centre.SetTranslation(gp_Vec(-(b.x0 + b.x1) / 2.0, -(b.y0 + b.y1) / 2.0, 0.0));
    TopoDS_Shape placed = BRepBuilderAPI_Transform(faces, centre, /*copy=*/true).Shape();
    if (std::abs(sx - sy) <= 1e-9 * std::max(sx, sy)) {
        gp_Trsf even;
        even.SetScaleFactor(sx);
        placed = BRepBuilderAPI_Transform(placed, even, true).Shape();
    } else {
        gp_GTrsf stretch;
        stretch.SetValue(1, 1, sx);
        stretch.SetValue(2, 2, sy);
        placed = BRepBuilderAPI_GTransform(placed, stretch, true).Shape();
    }
    BRepPrimAPI_MakePrism prism(placed, gp_Vec(0.0, 0.0, spec.height));
    if (!prism.IsDone())
        throw GeometryError("The letters could not be made solid");
    return prism.Shape();
}

TextSpec sizedForLetters(TextSpec spec, double letterHeight)
{
    if (!(letterHeight > 0.0))
        throw std::invalid_argument("The letters must be bigger than zero");
    const Box2 b = naturalBox(spec.text, spec.font, spec.bold, spec.italic);
    const double em = letterHeight / capitalRatio(spec.font, spec.bold, spec.italic);
    spec.width = b.width() * em / 10.0;
    spec.depth = b.height() * em / 10.0;
    return spec;
}

TextSpec withText(const TextSpec& old, const std::string& text, const std::string& font, bool bold, bool italic)
{
    TextSpec next = old;
    next.text = text;
    next.font = font;
    next.bold = bold;
    next.italic = italic;
    const Box2 now = naturalBox(text, font, bold, italic);  // throws for empty text
    double sx = 0.0, sy = 0.0;
    try {
        const Box2 was = naturalBox(old.text, old.font, old.bold, old.italic);
        sx = old.width / was.width();
        sy = old.depth / was.height();
    } catch (const std::exception&) {
    }
    if (!(sx > 0.0) || !(sy > 0.0) || !std::isfinite(sx) || !std::isfinite(sy))
        return sizedForLetters(next, 8.0);
    next.width = now.width() * sx;
    next.depth = now.height() * sy;
    return next;
}

double letterHeightOf(const TextSpec& spec)
{
    const Box2 b = naturalBox(spec.text, spec.font, spec.bold, spec.italic);
    return capitalRatio(spec.font, spec.bold, spec.italic) * 10.0 * spec.depth / b.height();
}

std::vector<FlatPiece> flatLetters(const EmbossText& words, double maxEdge)
{
    if (!(words.letterHeight > 0.0))
        throw std::invalid_argument("The letters must be bigger than zero");
    if (!(maxEdge > 0.0))
        throw std::invalid_argument("flatLetters: maxEdge must be above zero");
    const double em = words.letterHeight / capitalRatio(words.font, words.bold, words.italic);
    const TopoDS_Shape faces = letterFaces(words.text, words.font, words.bold, words.italic, em);
    const Box2 b = boundsOf(faces);
    const double cx = (b.x0 + b.x1) / 2.0, cy = (b.y0 + b.y1) / 2.0;
    const double a = words.angle * std::numbers::pi / 180.0, ca = std::cos(a), sa = std::sin(a);

    BRepMesh_IncrementalMesh mesher(faces, words.letterHeight / 400.0, /*relative=*/false, 0.2, /*parallel=*/false);
    std::vector<FlatPiece> out;
    for (TopExp_Explorer e(faces, TopAbs_FACE); e.More(); e.Next()) {
        const TopoDS_Face face = TopoDS::Face(e.Current());
        TopLoc_Location loc;
        const auto tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull() || tri->NbTriangles() == 0)
            continue;
        const gp_Trsf t = loc.Transformation();
        FlatPiece piece;
        for (int i = 1; i <= tri->NbNodes(); ++i) {
            const gp_Pnt p = tri->Node(i).Transformed(t);
            piece.points.push_back({p.X() - cx, p.Y() - cy});
        }
        for (int i = 1; i <= tri->NbTriangles(); ++i) {
            int n1 = 0, n2 = 0, n3 = 0;
            tri->Triangle(i).Get(n1, n2, n3);
            std::uint32_t v[3] = {std::uint32_t(n1 - 1), std::uint32_t(n2 - 1), std::uint32_t(n3 - 1)};
            const auto& p0 = piece.points[v[0]];
            const auto& p1 = piece.points[v[1]];
            const auto& p2 = piece.points[v[2]];
            const double area = (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p1[1] - p0[1]) * (p2[0] - p0[0]);
            if (std::abs(area) < 1e-14)
                continue;
            if (area < 0.0)
                std::swap(v[1], v[2]);
            piece.triangles.insert(piece.triangles.end(), {v[0], v[1], v[2]});
        }
        if (piece.triangles.empty())
            continue;
        refine(piece.points, piece.triangles, maxEdge);
        for (auto& p : piece.points)
            p = {p[0] * ca - p[1] * sa, p[0] * sa + p[1] * ca};
        out.push_back(std::move(piece));
    }
    if (out.empty())
        throw std::invalid_argument("Type some text to make letters");
    return out;
}

}  // namespace forge::text
