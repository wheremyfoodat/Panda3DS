package com.panda3ds.pandroid.data.node;

import androidx.annotation.NonNull;

import com.panda3ds.pandroid.lang.Function;

abstract class NodeBase {
    private NodeBase parent = null;
    private Function<NodeBase> changeListener;

    protected void setParent(NodeBase parent) {
        this.parent = parent;
    }

    protected void changed() {
        if (parent != null)
            parent.changed();
        if (changeListener != null)
            changeListener.run(this);
    }

    public <T extends NodeBase> void setChangeListener(Function<T> listener) {
        changeListener = val -> listener.run((T) val);
    }

    protected NodeBase getParent() {
        return parent;
    }

    protected abstract Object buildValue();

    @NonNull
    public abstract String toString();
}
