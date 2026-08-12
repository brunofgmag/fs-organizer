#ifndef FS_ORGANIZER_TESTS_SUPPORT_A_PDF_H
#define FS_ORGANIZER_TESTS_SUPPORT_A_PDF_H

#include <cstddef>
#include <string>
#include <vector>

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

[[nodiscard]] inline std::string APdfWhoseInfoSays(const std::string& info)
{
    const std::vector<std::string> objects = {"<</Type/Catalog/Pages 2 0 R>>", "<</Type/Pages/Kids[3 0 R]/Count 1>>",
                                              "<</Type/Page/Parent 2 0 R/MediaBox[0 0 200 200]>>", "<<" + info + ">>"};

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

    pdf += "trailer\n<</Size " + size + "/Root 1 0 R/Info 4 0 R>>\nstartxref\n" + std::to_string(startOfTheTable)
        + "\n%%EOF\n";

    return pdf;
}

[[nodiscard]] inline std::string AChartOfVersion(const long long version)
{
    return APdfWhoseInfoSays("/Title(" + AsAUtf16Literal("CV-" + std::to_string(version)) + ")");
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_A_PDF_H
