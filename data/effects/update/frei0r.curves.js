
var update = new Object();

update["Graph position"] = new Array(new Array(0.3, function(v, d) { return this.upd1(v, d); }));
update["Curve point number"] = new Array(new Array(0.2, function(v, d) { return this.upd1(v, d); }));

function upd1(value, isDowngrade) {
    if (isDowngrade)
        return value * 10;
    else
        return value / 10.;
}
