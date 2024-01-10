package com.panda3ds.pandroid.view.recycler;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;

public class SimpleListAdapter<T> extends RecyclerView.Adapter<SimpleListAdapter.Holder> {

    private final ArrayList<T> list = new ArrayList<>();
    private final Binder<T> binder;
    private final int layoutId;

    public SimpleListAdapter(@LayoutRes int layoutId, Binder<T> binder) {
        this.layoutId = layoutId;
        this.binder = binder;
    }

    @NonNull
    @Override
    public Holder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        return new Holder(LayoutInflater.from(parent.getContext()).inflate(layoutId, parent, false));
    }

    @Override
    public void onBindViewHolder(@NonNull Holder holder, int position) {
        binder.bind(position, list.get(position), holder.getView());
    }

    public void addAll(T... items) {
        addAll(Arrays.asList(items));
    }

    public void addAll(List<T> items) {
        int index = list.size();
        this.list.addAll(items);
        notifyItemRangeInserted(index, getItemCount() - index);
    }

    public void clear() {
        int count = getItemCount();
        list.clear();
        notifyItemRangeRemoved(0, count);
    }

    public void sort(Comparator<T> comparator) {
        list.sort(comparator);
        notifyItemRangeChanged(0, getItemCount());
    }

    @Override
    public int getItemCount() {
        return list.size();
    }

    public interface Binder<I> {
        void bind(int position, I item, View view);
    }

    public static class Holder extends RecyclerView.ViewHolder {
        public Holder(@NonNull View itemView) {
            super(itemView);
        }

        public View getView() {
            return itemView;
        }
    }
}
