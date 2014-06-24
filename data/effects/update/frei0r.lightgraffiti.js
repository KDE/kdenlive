
var update = new Object();

update["sensitivity"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d, 5.); }));
update["thresholdBrightness"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d, 765.); }));
update["thresholdDifference"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d, 255.); }));
update["thresholdDiffSum"] = update["thresholdBrightness"];
update["saturation"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d, 4.); }));
update["lowerOverexposure"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d, 10.); }));

function upd1(value, isDowngrade, factor) {
    var valueList = value.split(';');
    var locale = new QLocale();
    for (var i = 0; i < valueList.length; ++i) {
        var current = valueList[i].split('=');
        valueList[i] = current[0] + '=' + locale.toString(isDowngrade ? current[1] * factor : current[1] / factor);
    }
    return valueList.join(';');
}
