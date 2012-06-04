/*
 * This file is part of Arakoon, a distributed key-value store.
 *
 * Copyright (C) 2012 Incubaid BVBA
 *
 * Licensees holding a valid Incubaid license may use this file in
 * accordance with Incubaid's Arakoon commercial license agreement. For
 * more information on how to enter into this agreement, please contact
 * Incubaid (contact details can be found on http://www.arakoon.org/licensing).
 *
 * Alternatively, this file may be redistributed and/or modified under
 * the terms of the GNU Affero General Public License version 3, as
 * published by the Free Software Foundation. Under this license, this
 * file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Affero General Public License for more details.
 * You should have received a copy of the
 * GNU Affero General Public License along with this program (file "COPYING").
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ARAKOON_ASSERT_H__
#define __ARAKOON_ASSERT_H__

#include <errno.h>

#include "arakoon.h"

#ifndef ARAKOON_ASSERT
# define ASSERT_NON_NULL(v) \
        STMT_START          \
        STMT_END
# define ARAKOON_NON_NULL_RC(v) \
        STMT_START              \
        STMT_END
#else /* ifndef ARAKOON_ASSERT */
# define ASSERT_NON_NULL(v)                                            \
        STMT_START                                                     \
        if(ARAKOON_GNUC_UNLIKELY(!_arakoon_assert_non_null(            \
            v, ARAKOON_STRINGIFY(v), __FILE__, __LINE__, __func__))) { \
                errno = EINVAL;                                        \
                return NULL;                                           \
        }                                                              \
        STMT_END
# define ASSERT_NON_NULL_RC(v)                                         \
        STMT_START                                                     \
        if(ARAKOON_GNUC_UNLIKELY(!_arakoon_assert_non_null(            \
            v, ARAKOON_STRINGIFY(v), __FILE__, __LINE__, __func__))) { \
                return -EINVAL;                                        \
        }                                                              \
        STMT_END
#endif /* ifndef ARAKOON_ASSERT */

arakoon_bool _arakoon_assert_non_null(void *value, const char *name,
    const char *file, unsigned int line, const char *function);

#endif /* ifndef __ARAKOON_ASSERT_H__ */
