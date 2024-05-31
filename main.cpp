#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, size_t sz) {
    return path(data, data + sz);
}


bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream input(in_file);
    if (!input.is_open()) {
        cerr << "Failed to open input file: " << in_file << endl;
        return false;
    }

    ofstream output(out_file, ios_base::app);  // Append mode
    if (!output.is_open()) {
        cerr << "Failed to open output file: " << out_file << endl;
        return false;
    }

    string line;
    static regex include_local (R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex include_global (R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    size_t line_number = 0;

    while (getline(input, line)) {
        line_number++;
        smatch match;
        if (regex_match(line, match, include_local) || regex_match(line, match, include_global)) {
            path include_file = match[1].str();
            bool found = false;

            if (match[0].str().find('"') != string::npos) { // Local include
                path relative_path = in_file.parent_path() / include_file;
                if (exists(relative_path)) {
                    found = true;
                    if (!Preprocess(relative_path, out_file, include_directories)) {
                        return false;
                    }
                }
            }

            if (!found) { // Global include or local include not found
                for (const auto& dir : include_directories) {
                    path search_path = dir / include_file;
                    if (exists(search_path)) {
                        found = true;
                        if (!Preprocess(search_path, out_file, include_directories)) {
                            return false;
                        }
                        break;
                    }
                }
            }

            if (!found) {
                cout << "unknown include file " << include_file.string() << " at file " << in_file.string() << " at line " << line_number << endl;
                return false;
            }
        } else {
            output << line << endl;
        }
    }

    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                        {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}