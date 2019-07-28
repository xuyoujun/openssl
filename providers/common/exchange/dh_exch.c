/*
 * Copyright 2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>
#include <openssl/core_numbers.h>
#include <openssl/core_names.h>
#include <openssl/dh.h>
#include <openssl/params.h>
#include "internal/provider_algs.h"

static OSSL_OP_keyexch_newctx_fn dh_newctx;
static OSSL_OP_keyexch_init_fn dh_init;
static OSSL_OP_keyexch_set_peer_fn dh_set_peer;
static OSSL_OP_keyexch_derive_fn dh_derive;
static OSSL_OP_keyexch_freectx_fn dh_freectx;
static OSSL_OP_keyexch_dupctx_fn dh_dupctx;

/*
 * What's passed as an actual key is defined by the KEYMGMT interface.
 * We happen to know that our KEYMGMT simply passes DH structures, so
 * we use that here too.
 */

typedef struct {
    DH *dh;
    DH *dhpeer;
    int pad;
} PROV_DH_CTX;

static void *dh_newctx(void *provctx)
{
    return OPENSSL_zalloc(sizeof(PROV_DH_CTX));
}

static int dh_init(void *vpdhctx, void *vdh)
{
    PROV_DH_CTX *pdhctx = (PROV_DH_CTX *)vpdhctx;

    DH_free(pdhctx->dh);
    pdhctx->dh = vdh;
    DH_up_ref(pdhctx->dh);

    return pdhctx->dh != NULL;
}

static int dh_set_peer(void *vpdhctx, void *vdh)
{
    PROV_DH_CTX *pdhctx = (PROV_DH_CTX *)vpdhctx;

    DH_free(pdhctx->dhpeer);
    pdhctx->dhpeer = vdh;
    DH_up_ref(pdhctx->dhpeer);

    return pdhctx->dhpeer != NULL;
}

static int dh_derive(void *vpdhctx, unsigned char *key, size_t *keylen,
                     size_t outlen)
{
    PROV_DH_CTX *pdhctx = (PROV_DH_CTX *)vpdhctx;
    int ret;
    size_t dhsize;
    const BIGNUM *pub_key = NULL;

    /* TODO(3.0): Add errors to stack */
    if (pdhctx->dh == NULL || pdhctx->dhpeer == NULL)
        return 0;

    dhsize = (size_t)DH_size(pdhctx->dh);
    if (key == NULL) {
        *keylen = dhsize;
        return 1;
    }
    if (outlen < dhsize)
        return 0;

    DH_get0_key(pdhctx->dhpeer, &pub_key, NULL);
    ret = (pdhctx->pad) ? DH_compute_key_padded(key, pub_key, pdhctx->dh)
                        : DH_compute_key(key, pub_key, pdhctx->dh);
    if (ret <= 0)
        return 0;

    *keylen = ret;
    return 1;
}

static void dh_freectx(void *vpdhctx)
{
    PROV_DH_CTX *pdhctx = (PROV_DH_CTX *)vpdhctx;

    DH_free(pdhctx->dh);
    DH_free(pdhctx->dhpeer);

    OPENSSL_free(pdhctx);
}

static void *dh_dupctx(void *vpdhctx)
{
    PROV_DH_CTX *srcctx = (PROV_DH_CTX *)vpdhctx;
    PROV_DH_CTX *dstctx;

    dstctx = OPENSSL_zalloc(sizeof(*srcctx));

    *dstctx = *srcctx;
    if (dstctx->dh != NULL && !DH_up_ref(dstctx->dh)) {
        OPENSSL_free(dstctx);
        return NULL;
    }

    if (dstctx->dhpeer != NULL && !DH_up_ref(dstctx->dhpeer)) {
        DH_free(dstctx->dh);
        OPENSSL_free(dstctx);
        return NULL;
    }

    return dstctx;
}

static int dh_set_params(void *vpdhctx, const OSSL_PARAM params[])
{
    PROV_DH_CTX *pdhctx = (PROV_DH_CTX *)vpdhctx;
    const OSSL_PARAM *p;
    int pad;

    if (pdhctx == NULL || params == NULL)
        return 0;

    p = OSSL_PARAM_locate_const(params, OSSL_EXCHANGE_PARAM_PAD);
    if (p == NULL || !OSSL_PARAM_get_int(p, &pad))
        return 0;

    pdhctx->pad = pad;

    return 1;
}

const OSSL_DISPATCH dh_keyexch_functions[] = {
    { OSSL_FUNC_KEYEXCH_NEWCTX, (void (*)(void))dh_newctx },
    { OSSL_FUNC_KEYEXCH_INIT, (void (*)(void))dh_init },
    { OSSL_FUNC_KEYEXCH_DERIVE, (void (*)(void))dh_derive },
    { OSSL_FUNC_KEYEXCH_SET_PEER, (void (*)(void))dh_set_peer },
    { OSSL_FUNC_KEYEXCH_FREECTX, (void (*)(void))dh_freectx },
    { OSSL_FUNC_KEYEXCH_DUPCTX, (void (*)(void))dh_dupctx },
    { OSSL_FUNC_KEYEXCH_SET_PARAMS, (void (*)(void))dh_set_params },
    { 0, NULL }
};
