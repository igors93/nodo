import os, re

def find_issues():
    src_dir = r"c:\Users\Igor\Desktop\python\nodo\src"
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith(".cpp") or file.endswith(".hpp") or file.endswith(".h"):
                path = os.path.join(root, file)
                try:
                    with open(path, "r", encoding="utf-8") as f:
                        lines = f.readlines()
                    for i, line in enumerate(lines):
                        # 1. empty catch
                        if "catch" in line and "{" in line and "}" in line:
                            if re.search(r'catch\s*\(.*?\)\s*\{\s*\}', line):
                                print(f"Empty catch: {path}:{i+1} -> {line.strip()}")
                        # 2. lock_guard without var
                        if re.search(r'std::lock_guard<std::mutex>\s*\([^)]*\)\s*;', line):
                            print(f"Unnamed lock_guard: {path}:{i+1} -> {line.strip()}")
                        if re.search(r'std::lock_guard<std::mutex>\s+[a-zA-Z0-9_]+\s*;', line):
                            print(f"Uninitialized lock_guard: {path}:{i+1} -> {line.strip()}")
                        # 3. out of bounds static array
                        if re.search(r'\[.+\]', line) and "<" in line or "<=" in line:
                            pass # hard to regex
                except Exception as e:
                    pass

if __name__ == "__main__":
    find_issues()
