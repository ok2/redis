#include "redis.h"
#include "acl.h"

struct acl_t {
  char *expr;
  size_t exprlen;
  acl_mode_t mode;
  long long prio;
};

static void acl_free(struct acl_t *acl) {
  if (acl->expr != NULL) zfree(acl->expr);
  zfree(acl);
}

static struct acl_t *acl_alloc(char *expr,
                               struct acl_t *acl,
                               acl_mode_t mode,
                               long long prio)
{
  acl->mode = mode;
  acl->prio = prio;
  acl->expr = zstrdup(expr);
  acl->exprlen = strlen(expr);
  if (acl->expr == NULL)
    return NULL;
  return acl;
}

list *aclAdd(list *aclList, char *expr, acl_mode_t mode, long long prio) {
  struct acl_t *acl = zmalloc(sizeof(struct acl_t));
  if (!acl) goto error;
  if (!acl_alloc(expr, acl, mode, prio)) goto error;
  if (listAddNodeTail(aclList, acl) == NULL) goto error;
  return aclList;

 error:
  if (acl != NULL) acl_free(acl);
  return NULL;
}

static void *aclDup(void *ptr) {
  struct acl_t *prev = (struct acl_t *) ptr;
  struct acl_t *acl = zmalloc(sizeof(struct acl_t));
  if (acl == NULL) return NULL;
  if (!acl_alloc(prev->expr, acl, prev->mode, prev->prio)) {
    acl_free(acl);
    return NULL;
  }
  return acl;
}

static void aclFree(void *ptr) {
  acl_free((struct acl_t*) ptr);
}

static int aclMatch(void *ptr, void *key) {
  char *expr = (char*) key;
  struct acl_t *acl = (struct acl_t *) ptr;
  return strcmp(expr, acl->expr) == 0;
}

list *aclCreate() {
  list *acl;

  acl = listCreate();
  acl->free = aclFree;
  acl->dup = aclDup;
  acl->match = aclMatch;
  
  return acl;
}

list *aclFetch(list *acl, sds user, dict *db) {
  dictIterator *it = dictGetIterator(db);
  dictEntry *de = NULL;
  robj *val = NULL;
  sds key;
  acl_mode_t mode;
  long long prio = 0;
  char *str = NULL;
  size_t rlen = 0;

  while ((de = dictNext(it)) != NULL) {
    str = key = dictGetKey(de); rlen = sdslen(key);
    if (sdslen(key) < 6) continue;

    if (strncmp(str, "acl:", rlen < 4 ? rlen : 4) != 0) continue;
    str += 4; rlen -= 4;

    for (; str[0] != ':' && rlen > 0; str++, rlen--);
    ++str; --rlen; if (rlen < 1) continue;
    prio = strtoll(str, NULL, 10);
    if (prio < 0 || prio > 1000) continue;

    for (; str[0] != ':' && rlen > 0; str++, rlen--);
    ++str; --rlen; if (rlen < 5) continue;
    mode.n = 0;
    if (str[0] == 'r') mode.m.r = 1;
    if (str[1] == 'w') mode.m.w = 1;
    if (str[2] == 'l') mode.m.l = 1;
    if (str[3] != ':') continue;
    str += 4; rlen -= 4;
    if (!stringmatchlen(str, rlen, user, sdslen(user), 0))
      continue;
    val = dictGetVal(de);
    if (val->type == REDIS_STRING)
      acl = aclAdd(acl, val->ptr, mode, prio);
  }
  return acl;
}

int aclCheck(list *acl, sds key, acl_mode_t mode) {
  listIter *it = listGetIterator(acl, AL_START_HEAD);
  listNode *ln = NULL;
  long long maxprio = 0;
  char *maxexpr = NULL;
  acl_mode_t maxmode;

  while ((ln = listNext(it)) != NULL) {
    struct acl_t *a = (struct acl_t*) ln->value;
    if (a->prio < maxprio) continue;
    if (stringmatchlen(a->expr, a->exprlen, key, sdslen(key), 0))
      if ((a->prio >= maxprio)) {
        maxmode = a->mode;
        maxexpr = a->expr;
        maxprio = a->prio;
      }
  }
  if ((   mode.m.r && !maxmode.m.r)
      || (mode.m.w && !maxmode.m.w)
      || (mode.m.l && !maxmode.m.l))
    return 1;
  return 0;
}
