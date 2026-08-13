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

[[nodiscard]] inline std::string AManualOf(const int pages, const std::vector<ASectionOfAManual>& sections)
{
    const std::size_t firstPage = 4;
    const std::size_t firstSection = firstPage + static_cast<std::size_t>(pages);

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
        objects.emplace_back("<</Type/Page/Parent 2 0 R/MediaBox[0 0 200 200]>>");
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

    return APdfMadeOf(objects, {});
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_A_PDF_H
