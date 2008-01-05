
#include <QMouseEvent>
#include <QStylePainter>
#include <QLabel>
#include <QLayout>

#include <KDebug>
#include <KLocale>


#include "projectitem.h"
#include "timecode.h"

  const int NameRole = Qt::UserRole;
  const int DurationRole = NameRole + 1;
  const int FullPathRole = NameRole + 2;
  const int ClipTypeRole = NameRole + 3;

ProjectItem::ProjectItem(QTreeWidget * parent, const QStringList & strings, QDomElement xml, int clipId)
    : QTreeWidgetItem(parent, strings, QTreeWidgetItem::UserType), m_element(xml), m_clipType(DocClipBase::NONE), m_clipId(clipId)
{
  setSizeHint(0, QSize(65, 45));
  setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled);
  QString cType = m_element.attribute("type", 0);
  if (!cType.isEmpty()) {
    m_clipType = (DocClipBase::CLIPTYPE) cType.toInt();
    slotSetToolTip();
  }
  
}

ProjectItem::~ProjectItem()
{
}

int ProjectItem::clipId()
{
  return m_clipId;
}

QStringList ProjectItem::names()
{
  QStringList result;
  result.append(text(0));
  result.append(text(1));
  result.append(text(2));
  return result;
}

QDomElement ProjectItem::toXml()
{
    return m_element;
}

void ProjectItem::slotSetToolTip()
{
  QString tip = "<qt><b>";
  switch (m_clipType) {
    case 1:
      tip.append(i18n("Audio clip"));
      break;
    case 2:
      tip.append(i18n("Mute video clip"));
      break;
    case 3:
      tip.append(i18n("Video clip"));
      break;
    case 4:
      tip.append(i18n("Color clip"));
      setData(1, DurationRole, Timecode::getEasyTimecode(GenTime(m_element.attribute("out", "250").toInt(), 25), 25));
      break;
    case 5:
      tip.append(i18n("Image clip"));
      break;
    case 6:
      tip.append(i18n("Text clip"));
      break;
    case 7:
      tip.append(i18n("Slideshow clip"));
      break;
    case 8:
      tip.append(i18n("Virtual clip"));
      break;
    case 9:
      tip.append(i18n("Playlist clip"));
      break;
    default:
      tip.append(i18n("Unknown clip"));
    break;
  }

  setToolTip(1, tip);
}

void ProjectItem::setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata)
{
	if (attributes.contains("duration")) {
	    m_duration = GenTime(attributes["duration"].toInt(), 25);
	    setData(1, DurationRole, Timecode::getEasyTimecode(m_duration, 25));
	    m_durationKnown = true;
	} else {
	    // No duration known, use an arbitrary one until it is.
	    m_duration = GenTime(0.0);
	    m_durationKnown = false;
	}


	//extend attributes -reh
	if (attributes.contains("type")) {
	    if (attributes["type"] == "audio")
		m_clipType = DocClipBase::AUDIO;
	    else if (attributes["type"] == "video")
		m_clipType = DocClipBase::VIDEO;
	    else if (attributes["type"] == "av")
		m_clipType = DocClipBase::AV;
	    else if (attributes["type"] == "playlist")
		m_clipType = DocClipBase::PLAYLIST;
	} else {
	    m_clipType = DocClipBase::AV;
	}
	slotSetToolTip();

	if (m_element.isNull()) {
	  QDomDocument doc;
	  m_element = doc.createElement("producer");
	  m_element.setAttribute("resource", attributes["filename"]);
	  m_element.setAttribute("type", (int) m_clipType);
        }
/*
	if (attributes.contains("height")) {
	    m_height = attributes["height"].toInt();
	} else {
	    m_height = 0;
	}
	if (attributes.contains("width")) {
	    m_width = attributes["width"].toInt();
	} else {
	    m_width = 0;
	}
	//decoder name
	if (attributes.contains("name")) {
	    m_decompressor = attributes["name"];
	} else {
	    m_decompressor = "n/a";
	}
	//video type ntsc/pal
	if (attributes.contains("system")) {
	    m_system = attributes["system"];
	} else {
	    m_system = "n/a";
	}
	if (attributes.contains("fps")) {
	    m_framesPerSecond = attributes["fps"].toInt();
	} else {
	    // No frame rate known.
	    m_framesPerSecond = 0;
	}
	//audio attributes -reh
	if (attributes.contains("channels")) {
	    m_channels = attributes["channels"].toInt();
	} else {
	    m_channels = 0;
	}
	if (attributes.contains("format")) {
	    m_format = attributes["format"];
	} else {
	    m_format = "n/a";
	}
	if (attributes.contains("frequency")) {
	    m_frequency = attributes["frequency"].toInt();
	} else {
	    m_frequency = 0;
	}
	if (attributes.contains("videocodec")) {
	    m_videoCodec = attributes["videocodec"];
	}
	if (attributes.contains("audiocodec")) {
	    m_audioCodec = attributes["audiocodec"];
	}

	m_metadata = metadata;

	if (m_metadata.contains("description")) {
	    setDescription (m_metadata["description"]);
	}
	else if (m_metadata.contains("comment")) {
	    setDescription (m_metadata["comment"]);
	}
*/

}

#include "projectitem.moc"
