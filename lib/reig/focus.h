#ifndef REIG_FOCUS_H
#define REIG_FOCUS_H

#include "fwd.h"

namespace reig {
    class Focus {
    public:
        Focus() = default;

        Focus(const Focus& other) = delete;

        Focus(Focus&& other) noexcept = delete;

        Focus& operator=(const Focus& other) = delete;

        Focus& operator=(Focus&& other) noexcept = delete;

        int create_id();

        bool is_focused(int focusId) const;

        bool claim(int focusId);

        void release(int focusId);

    private:
        friend Context;

        void reset_counter();

        int mCurrentFocus = 0;
        unsigned mFocusCounter = 0;
    };
}

#endif //REIG_FOCUS_H
