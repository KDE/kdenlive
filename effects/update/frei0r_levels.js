
function update(serviceVersion, effectVersion, effectString) {
    var locale = new QLocale();
    var doc = new QDomDocument();
    doc.setContent(effectString);
    for (var node = doc.documentElement().firstChild(); !node.isNull(); node = node.nextSibling()) {
        var effectparam = node.toElement();
        if (effectparam.attribute("name") == "Channel" || effectparam.attribute("name") == "Histogram position") {
            if (serviceVersion < effectVersion) {
                // downgrade
                if (effectVersion > 0.1) {
                    effectparam.firstChild().toText().setData(locale.toString(effectparam.text() * 10));
                }
            } else {
                // upgrade
                if (effectVersion < 0.2) { 
                    effectparam.firstChild().toText().setData(locale.toString(effectparam.text() / 10.));
                }
            }
        }
    }
    return doc.toString();
}
