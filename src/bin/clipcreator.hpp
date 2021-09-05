/*
 *   SPDX-FileCopyrightText: 2017 Nicolas Carion *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef CLIPCREATOR_H
#define CLIPCREATOR_H

#include "definitions.h"
#include "undohelper.hpp"
#include <QString>
#include <memory>
#include <unordered_map>

class ProjectItemModel;

/** @brief This namespace provides convenience functions to create clips based on various parameters
 */
namespace ClipCreator {
/** @brief Create and inserts a color clip
   @param color : a string of the form "0xff0000ff" (solid red in RGBA)
   @param duration : duration expressed in number of frames
   @param name: name of the clip
   @param parentFolder: the binId of the containing folder
   @param model: a shared pointer to the bin item model
   @return the binId of the created clip
*/
QString createColorClip(const QString &color, int duration, const QString &name, const QString &parentFolder, const std::shared_ptr<ProjectItemModel> &model);

/** @brief Create a title clip
   @param properties : title properties (xmldata, etc)
   @param duration : duration of the clip
   @param name: name of the clip
   @param parentFolder: the binId of the containing folder
   @param model: a shared pointer to the bin item model
*/
QString createTitleClip(const std::unordered_map<QString, QString> &properties, int duration, const QString &name, const QString &parentFolder,
                        const std::shared_ptr<ProjectItemModel> &model);

/** @brief Create a title template
   @param path : path to the template
   @param text : text of the template (optional)
   @param name: name of the clip
   @param parentFolder: the binId of the containing folder
   @param model: a shared pointer to the bin item model
   @return the binId of the created clip
*/
QString createTitleTemplate(const QString &path, const QString &text, const QString &name, const QString &parentFolder,
                            const std::shared_ptr<ProjectItemModel> &model);

/** @brief Create a slideshow clip
   @param path : path to the selected folder
   @param duration: this should be nbr of images * duration of one image
   @param name: name of the clip
   @param parentFolder: the binId of the containing folder
   @param properties: description of the slideshow
   @param model: a shared pointer to the bin item model
   @return the binId of the created clip
*/
QString createSlideshowClip(const QString &path, int duration, const QString &name, const QString &parentFolder,
                            const std::unordered_map<QString, QString> &properties, const std::shared_ptr<ProjectItemModel> &model);
/** @brief Reads a file from disk and create the corresponding clip
   @param path : path to the file
   @param parentFolder: the binId of the containing folder
   @param model: a shared pointer to the bin item model
   @return the binId of the created clip
*/
QString createClipFromFile(const QString &path, const QString &parentFolder, const std::shared_ptr<ProjectItemModel> &model, Fun &undo, Fun &redo,
                           const std::function<void(const QString &)> &readyCallBack = [](const QString &) {});
bool createClipFromFile(const QString &path, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model);

/** @brief Iterates recursively through the given url list and add the files it finds, recreating a folder structure
   @param list: the list of items (can be folders)
   @param checkRemovable: if true, it will check if files are on removable devices, and warn the user if so
   @param parentFolder: the binId of the containing folder
   @param model: a shared pointer to the bin item model
 */
const QString createClipsFromList(const QList<QUrl> &list, bool checkRemovable, const QString &parentFolder, const std::shared_ptr<ProjectItemModel> &model, Fun &undo,
                         Fun &redo, bool topLevel = true);
const QString createClipsFromList(const QList<QUrl> &list, bool checkRemovable, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model);

/** @brief Create minimal xml description from an url
 */
QDomDocument getXmlFromUrl(const QString &path);
} // namespace ClipCreator

#endif
