#include "subtitlemodel.hpp"
#include "core.h"
#include "project/projectmanager.h"


SubtitleModel::SubtitleModel(std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
{

}

std::shared_ptr<SubtitleModel> SubtitleModel::getModel()
{
    return pCore->projectManager()->getSubtitleModel();
}
void SubtitleModel::parseSubtitle()
{
    //QModelIndex index;
    //int paramName = m_model->data(index, AssetParameterModel::NameRole).toInt();
    
    //QString filePath(m_asset->get(paramName.toUtf8().constData()));

    QString filePath= "path_to_srt_file.srt";
    QString start,end,comment;
    GenTime startPos, endPos;
    QString timeLine;
    int index =0, turn = 0,r =0;
    /*
     * turn = 0 -> Add subtitle number
     * turn = 1 -> Add string to timeLine
     * turn > 1 -> Add string to completeLine
     */
    if (filePath.isEmpty())
        return;
    if (filePath.contains(".srt"))
        qDebug()<< "srt File";
    QFile srtFile(filePath);
          
    if (!srtFile.exists() && !srtFile.open(QIODevice::ReadOnly)){
        qDebug() << " File not found " << filePath;
        return;
    }
    { //parsing srt file
        QTextStream stream(&srtFile);
        QString line;
        while (stream.readLineInto(&line)) {
            line = line.simplified();
            
            
	        if (line.compare(""))
	        {
	            if(!turn)
	            {
	                index=atoi(line.toStdString().c_str());
	                turn++;
	                continue;
	            }

	            if (line.contains("-->"))
	            {
	                timeLine += line;
	                
	                QStringList srtTime;
	                srtTime = timeLine.split(' ');
	                start = srtTime[0];
	                startPos= stringtoTime(start);
	                end = srtTime[2];
	                startPos= stringtoTime(start);

	            }
	            else
	            {
	                r++;
	                if (comment != "")
	                    comment += " ";
	                if(r == 1) // only one line
	                    comment += line;
	                else
	                    comment = comment + "\r" +line;
	            }

	            turn++;
	        }
	        else
            {
                addSubtitle(index,startPos,endPos,comment);
                //reinitialize
                comment = timeLine = "";
                turn = 0; r = 0;
            }
	    }
        
    }
   
}

GenTime SubtitleModel::stringtoTime(QString str)
{
    QStringList total,secs;
    int hours=0, mins=0, seconds=0, ms=0;
    double total_sec=0;
    total = str.split(':');
    hours = atoi(total[0].toStdString().c_str());
    mins = atoi(total[1].toStdString().c_str());

    secs = total[2].split(',');
    seconds = atoi(secs[0].toStdString().c_str());
    ms = atoi(secs[1].toStdString().c_str());
    total_sec= hours *3600 + mins *60 + seconds + ms * 0.001 ;
    GenTime pos= *new GenTime(total_sec);
    return pos;
}