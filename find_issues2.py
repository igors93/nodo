import os, re

def find_issues():
    src_dir = r"c:\Users\Igor\Desktop\python\nodo\src"
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith((".cpp", ".hpp", ".h")):
                path = os.path.join(root, file)
                try:
                    with open(path, "r", encoding="utf-8") as f:
                        content = f.read()
                    
                    # Missing virtual destructor
                    if "virtual " in content and "~" not in content and "class " in content:
                        print(f"Possible missing virtual destructor in: {path}")

                    # Empty if statement
                    if re.search(r'if\s*\([^)]*\)\s*\{\s*\}', content):
                        print(f"Empty if: {path}")

                    # throw without new or something?
                    pass
                except Exception as e:
                    pass

if __name__ == "__main__":
    find_issues()
