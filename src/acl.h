#ifndef __ACL_H__
#define __ACL_H__

#include "adlist.h"

typedef union {
  uint8_t n;
  struct {
    unsigned int r:1;
    unsigned int w:1;
    unsigned int l:1;
    unsigned int __unused4:1;
    unsigned int __unused5:1;
    unsigned int __unused6:1;
    unsigned int __unused7:1;
    unsigned int __unused8:1;
  } m;
} acl_mode_t;

static inline acl_mode_t aclrwl(uint8_t r, uint8_t w, uint8_t l) {
  acl_mode_t m;
  m.m.r = r;
  m.m.w = w;
  m.m.l = l;
  return m;
}

list *aclCreate();
list *aclAdd(list *, char *expr, acl_mode_t mode, long long prio);
list *aclFetch(list *, sds user, dict *db);
int aclCheck(list *, sds key, acl_mode_t mode);

#define aclRet(_C) do { addReply(_C, shared.notallowed); return; } while(0)
#define aclXn(_C, _X) do { if (_C->user != NULL _X) aclRet(_C); } while(0)
#define aclMn(_C, _N, _R, _W, _L) aclXn(_C,                             \
  && aclCheck(_C->acl, _C->argv[_N]->ptr, aclrwl(_R,_W,_L)) != 0)

#define aclFn(_C, _O, _R, _W, _L)                                       \
  (_C->user == NULL || aclCheck(_C->acl, _O, aclrwl(_R, _W, _L)) == 0)

#define aclC(_C) aclXn(_C,)
#define aclRn(_C, _N) aclMn(_C, _N, 1, 0, 0)
#define aclR1(_C) aclRn(_C, 1)
#define aclWn(_C, _N) aclMn(_C, _N, 0, 1, 0)
#define aclW1(_C) aclWn(_C, 1)
#define aclLn(_C, _N) aclMn(_C, _N, 0, 0, 1)
#define aclL1(_C) aclLn(_C, 1)
#define aclRWn(_C, _N) aclMn(_C, _N, 1, 1, 0)
#define aclRW1(_C) aclRn(_C, 1)

#endif
