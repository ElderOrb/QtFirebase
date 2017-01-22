unix {
    GIT_BRANCH_NAME = $$system(git rev-parse --abbrev-ref HEAD)
    message("QtFirebase branch $$GIT_BRANCH_NAME")
}

contains(QTFIREBASE_CONFIG,"analytics") {
    DEFINES += QTFIREBASE_BUILD_ANALYTICS
}

contains(QTFIREBASE_CONFIG,"admob") {
    DEFINES += QTFIREBASE_BUILD_ADMOB
}

DISTFILES += \
    $$PWD/LICENSE

# Currently supported Firebase targets
android|ios {
    include(qtfirebase_target.pri)
} else {
    include(qtfirebase_dummy.pri)
}
