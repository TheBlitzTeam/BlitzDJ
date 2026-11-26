package com.theblitzteam.blitzdj

import android.content.res.ColorStateList
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.DrawableRes
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import coil.load
import com.google.android.material.button.MaterialButton
import com.google.android.material.imageview.ShapeableImageView
import org.json.JSONArray
import org.json.JSONObject

data class TrackItem(
    val id: String,
    val uri: String,
    val name: String,
    val artist: String,
    val album: String,
    val imageUrl: String
) {
    companion object {
        fun from(json: JSONObject) = TrackItem(
            id = json.optString("id"),
            uri = json.optString("uri"),
            name = json.optString("name"),
            artist = json.optString("artist"),
            album = json.optString("album"),
            imageUrl = json.optString("image_url")
        )

        fun list(json: String): List<TrackItem> = runCatching {
            val array = JSONArray(json)
            (0 until array.length()).mapNotNull { index -> array.optJSONObject(index)?.let(::from) }
        }.getOrDefault(emptyList())
    }
}

data class DeviceItem(
    val key: String,
    val name: String,
    val subtitle: String,
    @DrawableRes val icon: Int,
    val highlighted: Boolean,
    val actionLabel: String?,
    val onAction: (() -> Unit)?
)

class TrackAdapter(
    private val onPlay: (TrackItem) -> Unit,
    private val onQueue: ((TrackItem) -> Unit)?
) : RecyclerView.Adapter<TrackAdapter.Holder>() {

    private var items: List<TrackItem> = emptyList()

    class Holder(view: View) : RecyclerView.ViewHolder(view) {
        val art: ShapeableImageView = view.findViewById(R.id.trackArt)
        val name: TextView = view.findViewById(R.id.trackName)
        val subtitle: TextView = view.findViewById(R.id.trackSubtitle)
        val primary: MaterialButton = view.findViewById(R.id.trackPrimary)
        val secondary: MaterialButton = view.findViewById(R.id.trackSecondary)
    }

    @android.annotation.SuppressLint("NotifyDataSetChanged")
    fun submit(list: List<TrackItem>) {
        items = list
        notifyDataSetChanged()
    }

    fun isEmpty(): Boolean = items.isEmpty()

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder =
        Holder(LayoutInflater.from(parent.context).inflate(R.layout.item_track, parent, false))

    override fun getItemCount(): Int = items.size

    override fun onBindViewHolder(holder: Holder, position: Int) {
        val track = items[position]

        holder.name.text = track.name
        holder.subtitle.text = if (track.album.isNotEmpty()) "${track.artist} • ${track.album}" else track.artist
        holder.art.load(track.imageUrl.ifEmpty { null }) {
            crossfade(true)
        }

        holder.itemView.setOnClickListener { onPlay(track) }
        holder.primary.setOnClickListener { onPlay(track) }

        val queue = onQueue
        holder.secondary.visibility = if (queue != null) View.VISIBLE else View.GONE
        holder.secondary.setOnClickListener { queue?.invoke(track) }
    }
}

class DeviceAdapter : RecyclerView.Adapter<DeviceAdapter.Holder>() {

    private var items: List<DeviceItem> = emptyList()

    class Holder(view: View) : RecyclerView.ViewHolder(view) {
        val icon: ShapeableImageView = view.findViewById(R.id.deviceIcon)
        val name: TextView = view.findViewById(R.id.deviceName)
        val subtitle: TextView = view.findViewById(R.id.deviceSubtitle)
        val action: MaterialButton = view.findViewById(R.id.deviceAction)
    }

    @android.annotation.SuppressLint("NotifyDataSetChanged")
    fun submit(list: List<DeviceItem>) {
        items = list
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder =
        Holder(LayoutInflater.from(parent.context).inflate(R.layout.item_device, parent, false))

    override fun getItemCount(): Int = items.size

    override fun onBindViewHolder(holder: Holder, position: Int) {
        val item = items[position]
        val context = holder.itemView.context
        val tint = ContextCompat.getColor(context, if (item.highlighted) R.color.accent else R.color.text_primary)

        holder.icon.setImageResource(item.icon)
        holder.icon.imageTintList = ColorStateList.valueOf(tint)
        holder.name.text = item.name
        holder.subtitle.text = item.subtitle
        holder.subtitle.setTextColor(
            ContextCompat.getColor(context, if (item.highlighted) R.color.accent else R.color.text_secondary)
        )

        val action = item.onAction

        if (item.actionLabel != null && action != null) {
            holder.action.visibility = View.VISIBLE
            holder.action.text = item.actionLabel
            holder.action.setOnClickListener { action() }
        } else {
            holder.action.visibility = View.GONE
            holder.action.setOnClickListener(null)
        }
    }
}
