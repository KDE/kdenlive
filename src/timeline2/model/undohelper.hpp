
using Fun = std::function<bool (void)>;
#define UPDATE_UNDO_REDO(operation, reverse, undo, redo)  \
    undo = [reverse, undo]() {                            \
        bool v = reverse();                               \
        return undo() && v;                               \
    };                                                    \
    redo = [operation, redo]() {                          \
        bool v = redo();                                  \
        return operation() && v;                          \
    };
