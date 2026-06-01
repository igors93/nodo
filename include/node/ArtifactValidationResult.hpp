#ifndef NODO_NODE_ARTIFACT_VALIDATION_RESULT_HPP
#define NODO_NODE_ARTIFACT_VALIDATION_RESULT_HPP

#include <string>

namespace nodo::node {

enum class ArtifactValidationFailureKind {
    NONE,
    INVALID_ARTIFACT,
    APPEND_FAILED
};

std::string artifactValidationFailureKindToString(
    ArtifactValidationFailureKind kind
);

class ArtifactValidationResult {
public:
    ArtifactValidationResult();

    static ArtifactValidationResult acceptedResult();

    static ArtifactValidationResult rejected(
        std::string reason
    );

    static ArtifactValidationResult appendRejected(
        std::string reason
    );

    bool accepted() const;
    const std::string& reason() const;
    ArtifactValidationFailureKind failureKind() const;
    bool appendFailed() const;

    std::string serialize() const;

private:
    bool m_accepted;
    std::string m_reason;
    ArtifactValidationFailureKind m_failureKind;
};

} // namespace nodo::node

#endif
