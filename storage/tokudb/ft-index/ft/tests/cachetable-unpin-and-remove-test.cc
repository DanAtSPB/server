/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1335  USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include "test.h"

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       void** UU(dd),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp       __attribute__((__unused__)),
       void *extraargs    __attribute__((__unused__))
       ) {
    *dirtyp = 0;
    *sizep = make_pair_attr(0);
    return 0;
}

// test simple unpin and remove
static void
cachetable_unpin_and_remove_test (int n) {
    if (verbose) printf("%s %d\n", __FUNCTION__, n);
    const int table_limit = 2*n;
    int r;
    int i;

    CACHETABLE ct;
    toku_cachetable_create(&ct, table_limit, ZERO_LSN, nullptr);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, 0777); assert(r == 0);
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);

    // generate some random keys
    CACHEKEY keys[n]; int nkeys = n;
    for (i=0; i<n; i++) {
        keys[i].b = random();
    }

    // put the keys into the cachetable
    for (i=0; i<n; i++) {
        uint32_t hi = toku_cachetable_hash(f1, make_blocknum(keys[i].b));
        toku_cachetable_put(f1, make_blocknum(keys[i].b), hi, (void *)(long) keys[i].b, make_pair_attr(1),wc, put_callback_nop);
    }
    
    // unpin and remove
    CACHEKEY testkeys[n];
    for (i=0; i<n; i++) testkeys[i] = keys[i];
    while (nkeys > 0) {
        i = random() % nkeys;
        uint32_t hi = toku_cachetable_hash(f1, make_blocknum(testkeys[i].b));
        r = toku_test_cachetable_unpin_and_remove(f1, testkeys[i], NULL, NULL);
        assert(r == 0);

        toku_cachefile_verify(f1);

        // verify that k is removed
        void *v;
        r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(testkeys[i].b), hi, PL_WRITE_EXPENSIVE, &v);
        assert(r != 0);

        testkeys[i] = testkeys[nkeys-1]; nkeys -= 1;
    }

    // verify that the cachtable is empty
    int nentries;
    toku_cachetable_get_state(ct, &nentries, NULL, NULL, NULL);
    assert(nentries == 0);

    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

// test remove when the pair in being written
static void
cachetable_put_evict_remove_test (int n) {
    if (verbose) printf("%s %d\n", __FUNCTION__, n);
    const int table_limit = n-1;
    int r;
    int i;

    CACHETABLE ct;
    toku_cachetable_create(&ct, table_limit, ZERO_LSN, nullptr);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, 0777); assert(r == 0);
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);

    uint32_t hi[n];
    for (i=0; i<n; i++)
        hi[i] = toku_cachetable_hash(f1, make_blocknum(i));

    // put 0, 1, 2, ... should evict 0
    for (i=0; i<n; i++) {
        toku_cachetable_put(f1, make_blocknum(i), hi[i], (void *)(long)i, make_pair_attr(1), wc, put_callback_nop);
        r = toku_test_cachetable_unpin(f1, make_blocknum(i), hi[i], CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }

    // get 0
    void *v; long s;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(0), hi[0], &v, &s, wc, fetch, def_pf_req_callback, def_pf_callback, true, 0);
    assert(r == 0);
        
    // remove 0
    r = toku_test_cachetable_unpin_and_remove(f1, make_blocknum(0), NULL, NULL);
    assert(r == 0);

    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_unpin_and_remove_test(8);
    cachetable_put_evict_remove_test(4);
    return 0;
}
