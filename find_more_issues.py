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
                        # Modulo or div by something that could be 0 without check
                        if " % 0" in line or " / 0" in line:
                            print(f"Div by zero: {path}:{i+1}")
                        
                        # Empty if
                        if re.search(r'if\s*\([^)]*\)\s*\{\s*\}', line):
                            print(f"Empty if: {path}:{i+1}")
                            
                        # .front() or .back()
                        if ".front()" in line or ".back()" in line:
                            # Contextual check could be complex, just print
                            pass
                        
                        # Catch by value instead of reference
                        if re.search(r'catch\s*\(\s*std::exception\s+[a-zA-Z_]+\s*\)', line):
                            print(f"Catch by value: {path}:{i+1}")

                        # string size checked incorrectly, e.g. .length < 0
                        if "< 0" in line and (".size()" in line or ".length()" in line):
                            print(f"Size < 0: {path}:{i+1}")

                        # Assignment in if
                        if re.search(r'if\s*\(\s*[a-zA-Z0-9_]+\s*=\s*[^=].*\)', line):
                            if "==" not in line and "!=" not in line and "<=" not in line and ">=" not in line:
                                print(f"Assignment in if: {path}:{i+1}")
                                
                except Exception as e:
                    pass

if __name__ == "__main__":
    find_issues()
