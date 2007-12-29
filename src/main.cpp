#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>
#include <KUrl> //new
 
#include "mainwindow.h"
 
int main (int argc, char *argv[])
{
  KAboutData aboutData( "kdenlive", "kdenlive",
      ki18n("Kdenlive"), "1.0",
      ki18n("A simple text area which can load and save."),
      KAboutData::License_GPL,
      ki18n("Copyright (c) 2007 Developer") );
  KCmdLineArgs::init( argc, argv, &aboutData );
 
  KCmdLineOptions options; //new
  options.add("+[file]", ki18n("Document to open")); //new
  KCmdLineArgs::addCmdLineOptions(options); //new
 
  KApplication app;
 
  MainWindow* window = new MainWindow();
  window->show();
 
  KCmdLineArgs *args = KCmdLineArgs::parsedArgs(); //new
  if(args->count()) //new
  {
    window->openFile(args->url(0).url()); //new
  }
 
  return app.exec();
}
