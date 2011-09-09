
var update = new Object();

update["diffspace"] = new Array(new Array(2.1, function(v, d) { return this.updDiff(v, d); }));
update["triplevel"] = new Array(new Array(2.1, function(v, d) { return this.updTrip(v, d); }));

function updDiff(value, isDowngrade) {
    if (isDowngrade)
        return value * 256;
    else
        return value / 256.;
}

function updTrip(value, isDowngrade) {
    if (isDowngrade)
        return 1 / (1 - value) - 1;
    else
        return 1 - 1 / (value + 1);
}
