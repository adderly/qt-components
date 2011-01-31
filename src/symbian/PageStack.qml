/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Components project on Qt Labs.
**
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions contained
** in the Technology Preview License Agreement accompanying this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
****************************************************************************/

// The PageStack item defines a container for pages and a stack-based
// navigation model. Pages can be defined as QML items or components.

import Qt 4.7
import com.nokia.symbian 1.0

import "PageStack.js" as Engine

Item {
    id: root

    width: parent ? parent.width : 0
    height: parent ? parent.height : 0

    property int depth: Engine.getDepth()
    property Item currentPage: null

    // Indicates whether there is an ongoing page transition.
    property bool busy: internal.ongoingTransitionCount > 0

    // Pushes a page on the stack.
    // The page can be defined as a component, item or string.
    // If an item is used then the page will get re-parented.
    // If a string is used then it is interpreted as a url that is used to load a page component.
    //
    // The page can also be given as an array of pages. In this case all those pages will be pushed
    // onto the stack. The items in the stack can be components, items or strings just like for single
    // pages. Additionally an object can be used, which specifies a page and an optional properties
    // property. This can be used to push multiple pages while still giving each of them properties.
    // When an array is used the transition animation will only be to the last page.
    //
    // The properties argument is optional and allows defining a map of properties to set on the page.
    // If the immediate argument is true then no transition animation is performed.
    // Returns the page instance.
    function push(page, properties, immediate) {
        return Engine.push(page, properties, false, immediate);
    }

    // Pops a page off the stack.
    // If page is specified then the stack is unwound to that page, to unwind to the first page specify
    // page as null. If the immediate argument is true then no transition animation is performed.
    // Returns the page instance that was popped off the stack.
    function pop(page, immediate) {
        return Engine.pop(page, immediate);
    }

    // Replaces a page on the stack.
    // See push() for details.
    function replace(page, properties, immediate) {
        return Engine.push(page, properties, true, immediate);
    }

    // Clears the page stack.
    function clear() {
        return Engine.clear();
    }

    // Iterates through all pages (top to bottom) and invokes the specified function.
    // If the specified function returns true the search stops and the find function
    // returns the page that the iteration stopped at. If the search doesn't result
    // in any page being found then null is returned.
    function find(func) {
        return Engine.find(func);
    }
    
    // Called when the page stack visibility changes.
    onVisibleChanged: {
        if (currentPage) {
            internal.setPageStatus(currentPage, visible ? Symbian.PageActive : Symbian.PageInactive);
            if (visible)
                currentPage.visible = currentPage.parent.visible = true;
        }
    }

    QtObject {
        id: internal

        // The number of ongoing transitions.
        property int ongoingTransitionCount: 0

        // Sets the page status.
        function setPageStatus(page, status) {
            if (page.status !== undefined) {
                if (status == Symbian.PageActive && page.status == Symbian.PageInactive)
                    page.status = Symbian.PageActivating;
                else if (status == Symbian.PageInactive && page.status == Symbian.PageActive)
                    page.status = Symbian.PageDeactivating;

                page.status = status;
            }
        }
    }

    // Component for page containers.
    Component {
        id: containerComponent

        Item {
            id: container

            width: parent ? parent.width : 0
            height: parent ? parent.height : 0

            // The states correspond to the different possible positions of the container.
            state: "Hidden"

            // The page held by this container.
            property Item page: null

            // The owner of the page.
            property Item owner: null

            // Duration of transition animation (in ms)
            property int transitionDuration: 500

            // Flag that indicates the container should be cleaned up after the transition has ended.
            property bool cleanupAfterTransition: false

            // Performs a push enter transition.
            function pushEnter(replace, immediate) {
                if (!immediate)
                    state = replace ? "Front" : "Right";
                state = "";
                page.visible = true;
                if (root.visible && immediate)
                    internal.setPageStatus(page, Symbian.PageActive);
            }

            // Performs a push exit transition.
            function pushExit(replace, immediate) {
                state = immediate ? "Hidden" : (replace ? "Back" : "Left");
                if (root.visible && immediate)
                    internal.setPageStatus(page, Symbian.PageInactive);
                if (replace) {
                    if (immediate)
                        cleanup();
                    else
                        cleanupAfterTransition = true;
                }
            }

            // Performs a pop enter transition.
            function popEnter(immediate) {
                if (!immediate)
                    state = "Left";
                state = "";
                page.visible = true;
                if (root.visible && immediate)
                    internal.setPageStatus(page, Symbian.PageActive);
            }

            // Performs a pop exit transition.
            function popExit(immediate) {
                state = immediate ? "Hidden" : "Right";
                if (root.visible && immediate)
                    internal.setPageStatus(page, Symbian.PageInactive);
                if (immediate)
                    cleanup();
                else
                    cleanupAfterTransition = true;
            }
            
            // Called when a transition has started.
            function transitionStarted() {
                internal.ongoingTransitionCount++;
                if (root.visible)
                    internal.setPageStatus(page, (state == "") ? Symbian.PageActivating : Symbian.PageDeactivating);
            }
            
            // Called when a transition has ended.
            function transitionEnded() {
                if (state != "")
                    state = "Hidden";
                if (root.visible)
                    internal.setPageStatus(page, (state == "") ? Symbian.PageActive : Symbian.PageInactive);
                    
                internal.ongoingTransitionCount--;
                if (cleanupAfterTransition)
                    cleanup();
            }

            states: [
                // Explicit properties for default state.
                State {
                    name: ""
                    PropertyChanges { target: container; visible: true }
                },
                // Start state for pop entry, end state for push exit.
                State {
                    name: "Left"
                    PropertyChanges { target: container; x: -width }
                },
                // Start state for push entry, end state for pop exit.
                State {
                    name: "Right"
                    PropertyChanges { target: container; x: width }
                },
                // Start state for replace entry.
                State {
                    name: "Front"
                    PropertyChanges { target: container; scale: 1.5; opacity: 0.0 }
                },
                // End state for replace exit.
                State {
                    name: "Back"
                    PropertyChanges { target: container; scale: 0.5; opacity: 0.0 }
                },
                // Inactive state.
                State {
                    name: "Hidden"
                    PropertyChanges { target: container; visible: false }
                }
            ]

            transitions: [
                // Pop entry and push exit transition.
                Transition {
                    from: ""; to: "Left"; reversible: true
                    SequentialAnimation {
                        ScriptAction { script: if (state == "Left") { transitionStarted(); } else { transitionEnded(); } }
                        PropertyAnimation { properties: "x"; easing.type: Easing.InOutExpo; duration: transitionDuration }
                        ScriptAction { script: if (state == "Left") { transitionEnded(); } else { transitionStarted(); } }
                    }
                },
                // Push entry and pop exit transition.
                Transition {
                    from: ""; to: "Right"; reversible: true
                    SequentialAnimation {
                        ScriptAction { script: if (state == "Right") { transitionStarted(); } else { transitionEnded(); } }
                        PropertyAnimation { properties: "x"; easing.type: Easing.InOutExpo; duration: transitionDuration }
                        ScriptAction { script: if (state == "Right") { transitionEnded(); } else { transitionStarted(); } }
                    }
                },
                // Replace entry transition.
                Transition {
                    from: "Front"; to: "";
                    SequentialAnimation {
                        ScriptAction { script: transitionStarted(); }
                        PropertyAnimation { properties: "scale,opacity"; easing.type: Easing.InOutExpo; duration: transitionDuration }
                        ScriptAction { script: transitionEnded(); }
                    }
                },
                // Replace exit transition.
                Transition {
                    from: ""; to: "Back";
                    SequentialAnimation {
                        ScriptAction { script: transitionStarted(); }
                        PropertyAnimation { properties: "scale,opacity"; easing.type: Easing.InOutExpo; duration: transitionDuration }
                        ScriptAction { script: transitionEnded(); }
                   }
                }
            ]
            
            // Cleans up the container and then destroys it.
            function cleanup() {
                if (page.status == Symbian.PageActive)
                    internal.setPageStatus(page, Symbian.PageInactive);
                if (owner != container) {
                    // container is not the owner of the page - re-parent back to original owner
                    page.visible = false;
                    page.parent = owner;
                }
                container.destroy();
            }

        }
    }

}
