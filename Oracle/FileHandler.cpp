#include "FileHandler.h"

cv::Mat LoadWithImdecode(const std::filesystem::path& p) {
    std::vector<uchar> buffer;
    {
        std::ifstream ifs(p, std::ios::binary);
        if (!ifs) {
            return {};
        }

        ifs.seekg(0, std::ios::end);
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        if (size <= 0) {
            return {};
        }

        buffer.resize(static_cast<size_t>(size));
        if (!ifs.read(reinterpret_cast<char*>(buffer.data()), size)) {
            return {};
        }
    }

    cv::Mat img = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);
    return img;
}

void SaveReferencePiece(const cv::Mat& img, const cv::Rect& roi, const std::string& path) {
    cv::Rect bounded = roi & cv::Rect(0, 0, img.cols, img.rows);
    if (bounded.width <= 0 || bounded.height <= 0) return;
    cv::Mat crop = img(bounded).clone();

    crop = ApplyPaletteMasking(crop);

    cv::imwrite((g_tempDir / path).string(), crop);
}

std::map<std::string, cv::Mat> LoadReferencePieces(const std::filesystem::path& tempDir) {
    std::map<std::string, cv::Mat> refs;

    auto absTempDir = std::filesystem::absolute(tempDir);
    if (!std::filesystem::exists(absTempDir)) {
        return refs;
    }

    for (const auto& entry : std::filesystem::directory_iterator(absTempDir)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        if (ext != ".png" && ext != ".PNG") continue;

        auto stem = entry.path().stem().string();

        cv::Mat img = LoadWithImdecode(entry.path());
        if (img.empty()) {
            continue;
        }

        refs[stem] = img;
    }

    return refs;
}