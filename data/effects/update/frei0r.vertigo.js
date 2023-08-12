// SPDX-FileCopyrightText: Till Theato <root@ttill.de>
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

var update = new Object();

update["Zoomrate"] = new Array(new Array(1.1, function(v, d) { return this.upd1(v, d); }));

function upd1(value, isDowngrade) {
    var valueList = value.split(';');
    var locale = new QLocale();
    for (var i = 0; i < valueList.length; ++i) {
        var current = valueList[i].split('=');
        valueList[i] = current[0] + '=' + locale.toString(isDowngrade ? current[1] * 5 : current[1] / 5.);
    }
    return valueList.join(';');
}
