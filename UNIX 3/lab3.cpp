#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <iomanip>
#include <openssl/sha.h>

using namespace std;
namespace fs = std::filesystem;
string compute_sha1(const fs::path& filepath) {
    ifstream file(filepath, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Не удалось открыть файл: " + filepath.string());
    }

    SHA_CTX sha1;
    SHA1_Init(&sha1);

    char buffer[4096];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        SHA1_Update(&sha1, buffer, file.gcount());
    }

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &sha1);

    stringstream ss;
    ss << hex << setfill('0');
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << setw(2) << static_cast<int>(hash[i]);
    }

    return ss.str();
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "");

    if (argc < 2) {
        cerr << "Использование: " << argv[0] << " <путь_к_каталогу>" << endl;
        return 1;
    }

    fs::path target_dir = argv[1];

    if (!fs::exists(target_dir) || !fs::is_directory(target_dir)) {
        cerr << "Ошибка: Путь не существует или это не директория." << endl;
        return 1;
    }

    unordered_map<string, fs::path> seen_hashes;
    
    size_t files_processed = 0;
    size_t duplicates_replaced = 0;
    size_t errors_count = 0;

    cout << "Сканирование директории: " << target_dir << endl;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(target_dir)) {
            if (!entry.is_regular_file() || entry.is_symlink()) {
                continue;
            }

            fs::path current_path = entry.path();
            files_processed++;

            try {
                string hash = compute_sha1(current_path);

                auto it = seen_hashes.find(hash);

                if (it == seen_hashes.end()) {

                    seen_hashes[hash] = current_path;
                } else {

                    fs::path original_path = it->second;

                    if (fs::equivalent(current_path, original_path)) {
                        continue; 
                    }

                    cout << "[ДУБЛИКАТ] " << current_path.filename() << " == " << original_path.filename() << endl;

                    fs::remove(current_path);
                    
                    fs::create_hard_link(original_path, current_path);
                    
                    duplicates_replaced++;
                }
            } catch (const exception& ex) {
                cerr << "Ошибка с файлом " << current_path.filename() << ": " << ex.what() << endl;
                errors_count++;
            }
        }
    } catch (const fs::filesystem_error& ex) {
        cerr << "Критическая ошибка файловой системы: " << ex.what() << endl;
        return 1;
    }

    cout << "Всего проверено файлов: " << files_processed << endl;
    cout << "Заменено на жесткие ссылки: " << duplicates_replaced << endl;
    if (errors_count > 0) cout << "Ошибок доступа: " << errors_count << endl;

    return 0;
}
