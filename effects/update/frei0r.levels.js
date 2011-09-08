
var update = new Object();

update["Channel"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d); }));
update["Histogram position"] = update["Channel"];

function upd1(value, isDowngrade) {
    if (isDowngrade)
        return value * 10;
    else
        return value / 10.;
}
