import os, re

def find_issues():
    src_dir = r"c:\Users\Igor\Desktop\python\nodo\src"
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith((".cpp", ".hpp", ".h")):
                path = os.path.join(root, file)
                try:
                    with open(path, "r", encoding="utf-8") as f:
                        lines = f.readlines()
                    
                    for i, line in enumerate(lines):
                        line = line.strip()
                        if line.startswith("#"):
                            continue
                        if "//" in line:
                            line = line.split("//")[0]
                        
                        match = re.search(r'[\/%]\s*([a-zA-Z_][a-zA-Z0-9_]*)', line)
                        if match:
                            var = match.group(1)
                            # Exclude U suffix or standard constants
                            if var not in ("U", "f", "npos", "sizeof", "std", "reinterpret_cast", "static_cast", "UNITS_PER_NODO"):
                                print(f"Div by {var} in {path}:{i+1} -> {line}")
                except Exception:
                    pass

if __name__ == "__main__":
    find_issues()
