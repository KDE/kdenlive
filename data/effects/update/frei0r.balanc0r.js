// SPDX-FileCopyrightText: Till Theato <root@ttill.de>
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

var update = new Object();

update["Green Tint"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d); }));

function upd1(value, isDowngrade) {
    if (isDowngrade)
        return value 1.0 + (2.5 - 1.0) * value;
    else
        return (value - 1.0) / (2.5 - 1.0);
}
