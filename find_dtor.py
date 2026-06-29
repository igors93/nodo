import os, re

def find_issues():
    src_dir = r"c:\Users\Igor\Desktop\python\nodo\src"
    inc_dir = r"c:\Users\Igor\Desktop\python\nodo\include"
    for d in [src_dir, inc_dir]:
        for root, dirs, files in os.walk(d):
            for file in files:
                if file.endswith((".cpp", ".hpp", ".h")):
                    path = os.path.join(root, file)
                    try:
                        with open(path, "r", encoding="utf-8") as f:
                            content = f.read()
                        
                        # Find classes
                        for match in re.finditer(r'class\s+([A-Z][a-zA-Z0-9_]*).*?\{', content, re.DOTALL):
                            class_name = match.group(1)
                            class_body = content[match.end():]
                            # heuristic: find next closing brace? too hard with regex.
                            # Just check if the file contains "virtual " and "~" + class_name
                            if "virtual " in content:
                                if f"~{class_name}" in content and f"virtual ~{class_name}" not in content:
                                    print(f"Missing virtual destructor in {class_name} in {path}")
                    except Exception:
                        pass

if __name__ == "__main__":
    find_issues()
