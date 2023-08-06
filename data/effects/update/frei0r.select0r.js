// SPDX-FileCopyrightText: Till Theato <root@ttill.de>
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

var update = new Object();

update["Edge mode"] = new Array(new Array(0.4, function(v, d) { return this.upd1(v, d); }));

function upd1(value, isDowngrade) {
    if (isDowngrade) {
        if (value == 0.7)
            return 1.0;
    } else {
        if (value == 1.0)
            return 0.7;
    }
    return value;
}
