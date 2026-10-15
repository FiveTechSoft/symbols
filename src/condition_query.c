#include "condition_query.h"
#include "tl_sha256.h"
#include <string.h>

CQ_STATUS ConditionQuery(const unsigned char *question, size_t question_size,
                         const unsigned char *source, size_t source_size,
                         const unsigned char expected_source_sha256[32],
                         CQ_RESULT *out)
{
    CM_REQUIREMENT requirement;
    CM_CLAIM claims[CM_CLAIM_MAX];
    CM_SOURCE image;
    CM_RESULT match;
    CM_STATUS status;
    size_t count=0;
    unsigned char hash[32];
    if(!out) return CQ_INVALID;
    memset(out,0,sizeof(*out));
    out->version=CQ_VERSION;
    if(!source || !source_size || source_size>CP_SOURCE_MAX ||
       !expected_source_sha256) return CQ_INVALID;
    TLSha256(source,source_size,hash);
    if(memcmp(hash,expected_source_sha256,32) ||
       ConditionParseQuestion(question,question_size,&requirement)!=CP_PARSED ||
       ConditionExtractClaims(source,source_size,claims,CM_CLAIM_MAX,&count)!=CP_PARSED)
        return CQ_INVALID;
    image.bytes=source; image.size=source_size;
    status=ConditionMatch(&requirement,claims,count,&image,1,&match);
    if(status==CM_MATCH || status==CM_REFUTED) {
        const CM_CLAIM *claim;
        if(match.claim_index>=count) return CQ_INVALID;
        claim=&claims[match.claim_index];
        out->status=status==CM_MATCH?CQ_ASSERTION_MATCH:CQ_ASSERTION_REFUTED;
        memcpy(out->object,match.object,sizeof(out->object));
        out->origin=claim->origin;
        memcpy(out->source_sha256,claim->source_sha256,32);
        out->first=claim->first; out->end=claim->end;
    } else if(status==CM_CONFLICT) out->status=CQ_CONFLICT;
    else if(status==CM_UNDETERMINED) out->status=CQ_UNKNOWN;
    return out->status;
}
