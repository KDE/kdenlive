# Contributing to Kdenlive

* Create a merge request on GitLab
* Get it reviewed
* Join the Telegram group


## Coding Guidelines

* Use existing code style
* Write unit tests
* Document unclear parts (e.g. what a QMutex is locking)


## Release + Branching Model

Kdenlive is following the [KDE Release Schedule][sched] and a new major version
like `20.04` is released every four months. Bugfix releases like `20.04.1` are
released until the next major release arrives.

The branching model in Git works as follows:

* `master` is the most recent development version.
* `release/*` contains release branches like `release/20.04`. Shortly before a
  new major version is released, the new release branch is created and we enter
  feature and string freeze for final bugfixes and translation of texts. The
  version which is effectively released is marked with a tag.
* `feature/*` contains feature branches. When you start coding a new feature,
  do it on a feature branch. When it is ready, the feature branch is merged
  back into master if the merge request has been approved.

Bugfixes for the released versions are added on the release branch and then
merged back to master.

[sched]: https://community.kde.org/Schedules
