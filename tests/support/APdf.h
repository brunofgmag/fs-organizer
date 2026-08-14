#ifndef FS_ORGANIZER_TESTS_SUPPORT_A_PDF_H
#define FS_ORGANIZER_TESTS_SUPPORT_A_PDF_H

#include <cstddef>
#include <string>
#include <vector>

struct ASectionOfAManual
{
    std::string title{};
    int page = 0;
};

[[nodiscard]] inline std::string TenDigitsOf(const std::size_t offset)
{
    const std::string digits = std::to_string(offset);

    return std::string(10 - digits.size(), '0') + digits;
}

[[nodiscard]] inline std::string AsAUtf16Literal(const std::string& text)
{
    std::string escaped = "\\376\\377";

    for (const char letter : text)
    {
        escaped += "\\000";
        escaped += letter;
    }

    return escaped;
}

[[nodiscard]] inline std::string APdfMadeOf(const std::vector<std::string>& objects,
                                            const std::string& alsoInTheTrailer)
{
    std::string pdf = "%PDF-1.4\n";
    std::vector<std::size_t> offsets;

    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        offsets.push_back(pdf.size());
        pdf += std::to_string(index + 1) + " 0 obj\n" + objects[index] + "\nendobj\n";
    }

    const std::size_t startOfTheTable = pdf.size();
    const std::string size = std::to_string(objects.size() + 1);

    pdf += "xref\n0 " + size + "\n0000000000 65535 f \n";

    for (const std::size_t offset : offsets)
    {
        pdf += TenDigitsOf(offset) + " 00000 n \n";
    }

    pdf += "trailer\n<</Size " + size + "/Root 1 0 R" + alsoInTheTrailer + ">>\nstartxref\n"
        + std::to_string(startOfTheTable) + "\n%%EOF\n";

    return pdf;
}

[[nodiscard]] inline std::string APdfWhoseInfoSays(const std::string& info)
{
    return APdfMadeOf({"<</Type/Catalog/Pages 2 0 R>>", "<</Type/Pages/Kids[3 0 R]/Count 1>>",
                       "<</Type/Page/Parent 2 0 R/MediaBox[0 0 200 200]>>", "<<" + info + ">>"},
                      "/Info 4 0 R");
}

[[nodiscard]] inline std::string AChartOfVersion(const long long version)
{
    return APdfWhoseInfoSays("/Title(" + AsAUtf16Literal("CV-" + std::to_string(version)) + ")");
}

inline const std::string kOnEveryPage = "flight manual";

[[nodiscard]] inline std::string AManualOf(const int pages, const std::vector<ASectionOfAManual>& sections)
{
    const std::size_t firstPage = 4;
    const std::size_t firstSection = firstPage + static_cast<std::size_t>(pages);
    const std::size_t theInk = firstSection + sections.size();
    const std::size_t theFont = theInk + 1;

    std::string kids;
    for (int page = 0; page < pages; ++page)
    {
        kids += std::to_string(firstPage + static_cast<std::size_t>(page)) + " 0 R ";
    }

    const bool itCarriesAnOutline = !sections.empty();

    std::string outlines = "<<>>";
    if (itCarriesAnOutline)
    {
        outlines = "<</Type/Outlines/First " + std::to_string(firstSection) + " 0 R/Last "
            + std::to_string(firstSection + sections.size() - 1) + " 0 R/Count " + std::to_string(sections.size())
            + ">>";
    }

    std::string catalog = "<</Type/Catalog/Pages 2 0 R>>";
    if (itCarriesAnOutline)
    {
        catalog = "<</Type/Catalog/Pages 2 0 R/Outlines 3 0 R>>";
    }

    std::vector<std::string> objects = {
        catalog, "<</Type/Pages/Kids[" + kids + "]/Count " + std::to_string(pages) + ">>", outlines};

    for (int page = 0; page < pages; ++page)
    {
        objects.emplace_back("<</Type/Page/Parent 2 0 R/MediaBox[0 0 200 200]/Contents " + std::to_string(theInk)
                             + " 0 R/Resources<</Font<</F1 " + std::to_string(theFont) + " 0 R>>>>>>");
    }

    for (std::size_t which = 0; which < sections.size(); ++which)
    {
        const std::size_t me = firstSection + which;
        const std::size_t target = firstPage + static_cast<std::size_t>(sections[which].page);

        std::string item = "<</Title(" + sections[which].title + ")/Parent 3 0 R/Count 0/Dest[" + std::to_string(target)
            + " 0 R/XYZ null null null]";

        if (which > 0)
        {
            item += "/Prev " + std::to_string(me - 1) + " 0 R";
        }

        if (which + 1 < sections.size())
        {
            item += "/Next " + std::to_string(me + 1) + " 0 R";
        }

        objects.push_back(item + ">>");
    }

    const std::string drawn = "BT /F1 12 Tf 20 100 Td (" + kOnEveryPage + ") Tj ET";

    objects.push_back("<</Length " + std::to_string(drawn.size()) + ">>\nstream\n" + drawn + "\nendstream");
    objects.emplace_back("<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>");

    return APdfMadeOf(objects, {});
}

[[nodiscard]] inline std::string AManualWhoseFirstPageIsALinkToTheSecond()
{
    const std::string kStreamHead = "\nstream\n";
    const std::string kStreamTail = "\nendstream";
    const std::string drawn = "BT /F1 12 Tf 20 100 Td (" + kOnEveryPage + ") Tj ET";

    return APdfMadeOf(
        {"<</Type/Catalog/Pages 2 0 R>>", "<</Type/Pages/Kids[3 0 R 4 0 R]/Count 2>>",
         "<</Type/Page/Parent 2 0 R/MediaBox[0 0 200 200]/Annots[5 0 R]/Contents 6 0 R/Resources<</Font<</F1 7 0 "
         "R>>>>>>",
         "<</Type/Page/Parent 2 0 R/MediaBox[0 0 200 200]/Contents 6 0 R/Resources<</Font<</F1 7 0 R>>>>>>",
         "<</Type/Annot/Subtype/Link/Rect[0 0 200 200]/Border[0 0 0]/Dest[4 0 R/XYZ null null null]>>",
         "<</Length " + std::to_string(drawn.size()) + ">>" + kStreamHead + drawn + kStreamTail,
         "<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>"},
        {});
}

[[nodiscard]] inline std::string ATallPageWhereTheTermRepeats(const int times)
{
    const int kTall = 4000;
    const int kApart = 400;

    std::string drawn;
    for (int which = 0; which < times; ++which)
    {
        drawn +=
            "BT /F1 12 Tf 20 " + std::to_string(kTall - kApart * (which + 1)) + " Td (" + kOnEveryPage + ") Tj ET\n";
    }

    return APdfMadeOf({"<</Type/Catalog/Pages 2 0 R>>", "<</Type/Pages/Kids[3 0 R]/Count 1>>",
                       "<</Type/Page/Parent 2 0 R/MediaBox[0 0 200 " + std::to_string(kTall)
                           + "]/Contents 4 0 R/Resources<</Font<</F1 5 0 R>>>>>>",
                       "<</Length " + std::to_string(drawn.size()) + ">>\nstream\n" + drawn + "\nendstream",
                       "<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>"},
                      {});
}

[[nodiscard]] inline std::vector<std::string> TheLinesDrawnOn(const int page)
{
    const std::string mark = "p" + std::to_string(page);

    return {mark + " alpha bravo", mark + " charlie delta", mark + " echo foxtrot"};
}

[[nodiscard]] inline std::string AManualWhoseLinesAreKnown(const int pages)
{
    const std::size_t firstPage = 3;
    const std::size_t firstStream = firstPage + static_cast<std::size_t>(pages);
    const std::size_t theFont = firstStream + static_cast<std::size_t>(pages);

    std::string kids;
    for (int page = 0; page < pages; ++page)
    {
        kids += std::to_string(firstPage + static_cast<std::size_t>(page)) + " 0 R ";
    }

    std::vector<std::string> objects = {"<</Type/Catalog/Pages 2 0 R>>",
                                        "<</Type/Pages/Kids[" + kids + "]/Count " + std::to_string(pages) + ">>"};

    for (int page = 0; page < pages; ++page)
    {
        objects.emplace_back("<</Type/Page/Parent 2 0 R/MediaBox[0 0 200 200]/Contents "
                             + std::to_string(firstStream + static_cast<std::size_t>(page))
                             + " 0 R/Resources<</Font<</F1 " + std::to_string(theFont) + " 0 R>>>>>>");
    }

    for (int page = 0; page < pages; ++page)
    {
        const std::vector<std::string> lines = TheLinesDrawnOn(page);

        std::string drawn;
        int baseline = 160;

        for (const std::string& line : lines)
        {
            drawn += "BT /F1 12 Tf 20 " + std::to_string(baseline) + " Td (" + line + ") Tj ET\n";
            baseline -= 20;
        }

        objects.push_back("<</Length " + std::to_string(drawn.size()) + ">>\nstream\n" + drawn + "\nendstream");
    }

    objects.emplace_back("<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>");

    return APdfMadeOf(objects, {});
}

[[nodiscard]] inline std::string AManualWhosePagesWidenHalfway(const int pages)
{
    const std::size_t firstPage = 3;
    const std::size_t theInk = firstPage + static_cast<std::size_t>(pages);
    const std::size_t theFont = theInk + 1;

    std::string kids;
    for (int page = 0; page < pages; ++page)
    {
        kids += std::to_string(firstPage + static_cast<std::size_t>(page)) + " 0 R ";
    }

    std::vector<std::string> objects = {"<</Type/Catalog/Pages 2 0 R>>",
                                        "<</Type/Pages/Kids[" + kids + "]/Count " + std::to_string(pages) + ">>"};

    for (int page = 0; page < pages; ++page)
    {
        const std::string wide = page < pages / 2 ? "200" : "400";

        objects.emplace_back("<</Type/Page/Parent 2 0 R/MediaBox[0 0 " + wide + " 200]/Contents "
                             + std::to_string(theInk) + " 0 R/Resources<</Font<</F1 " + std::to_string(theFont)
                             + " 0 R>>>>>>");
    }

    const std::string drawn = "BT /F1 12 Tf 20 100 Td (" + kOnEveryPage + ") Tj ET";

    objects.push_back("<</Length " + std::to_string(drawn.size()) + ">>\nstream\n" + drawn + "\nendstream");
    objects.emplace_back("<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>");

    return APdfMadeOf(objects, {});
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_A_PDF_H
