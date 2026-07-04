import sys

with open('src/p2p/PeerRateLimiter.cpp', 'r') as f:
    content = f.read()

content = content.replace(
    'return true;\n}',
    'return true;\n}\n#include <iostream>'
).replace(
    'if (it->second.count >= limit) {',
    'std::cerr << "[TEST] shouldAllow nodeId=" << nodeId << " type=" << (int)messageType << " count=" << it->second.count << " limit=" << limit << " windowStart=" << it->second.windowStart << " now=" << now << "\\n";\n  if (it->second.count >= limit) {'
)

with open('src/p2p/PeerRateLimiter.cpp', 'w') as f:
    f.write(content)

