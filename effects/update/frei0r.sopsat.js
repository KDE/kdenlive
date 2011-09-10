
var update = new Object();

update["rSlope"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d, 20., 0); }));
update["gSlope"] = update["rSlope"];
update["bSlope"] = update["rSlope"];
update["aSlope"] = update["rSlope"];
update["rOffset"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d, 8., -4); }));
update["gOffset"] = update["rOffset"];
update["bOffset"] = update["rOffset"];
update["aOffset"] = update["rOffset"];
update["rPower"] = update["rSlope"];
update["gPower"] = update["rSlope"];
update["bPower"] = update["rSlope"];
update["aPower"] = update["rSlope"];
update["saturation"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d, 10., 0); }));

function upd1(value, isDowngrade, factor, offset) {
    var valueList = value.split(';');
    var locale = new QLocale();
    for (var i = 0; i < valueList.length; ++i) {
        var current = valueList[i].split('=');
        valueList[i] = current[0] + '=' + locale.toString(isDowngrade ? offset + current[1] * factor : (current[1] - offset) / factor);
    }
    return valueList.join(';');
}
