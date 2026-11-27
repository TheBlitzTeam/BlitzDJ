package com.theblitzteam.blitzdj

import android.content.Intent
import android.nfc.NdefMessage
import android.nfc.NdefRecord
import android.nfc.NfcAdapter
import androidx.core.content.IntentCompat

object NfcHandover {
    const val CLASSIC_MIME = "application/vnd.bluetooth.ep.oob"
    const val LOW_ENERGY_MIME = "application/vnd.bluetooth.le.oob"

    private const val TYPE_SHORT_NAME = 0x08
    private const val TYPE_COMPLETE_NAME = 0x09
    private const val TYPE_LE_ADDRESS = 0x1B

    data class Target(val address: String, val name: String?)

    fun isHandover(intent: Intent?): Boolean =
        intent?.action == NfcAdapter.ACTION_NDEF_DISCOVERED

    fun parse(intent: Intent): Target? {
        val messages = IntentCompat.getParcelableArrayExtra(intent, NfcAdapter.EXTRA_NDEF_MESSAGES, NdefMessage::class.java)
            ?: return null

        for (message in messages) {
            val records = (message as? NdefMessage)?.records ?: continue

            for (record in records) {
                if (record.tnf != NdefRecord.TNF_MIME_MEDIA) {
                    continue
                }

                val type = String(record.type, Charsets.US_ASCII).lowercase()
                val payload = record.payload ?: continue

                val target = when (type) {
                    CLASSIC_MIME -> parseClassic(payload)
                    LOW_ENERGY_MIME -> parseLowEnergy(payload)
                    else -> null
                }

                if (target != null) {
                    return target
                }
            }
        }

        return null
    }

    private fun parseClassic(payload: ByteArray): Target? {
        if (payload.size < 8) {
            return null
        }

        val address = formatAddress(payload, 2)
        return Target(address, findName(payload, 8))
    }

    private fun parseLowEnergy(payload: ByteArray): Target? {
        var address: String? = null

        forEachStructure(payload, 0) { type, offset, length ->
            if (type == TYPE_LE_ADDRESS && length >= 6) {
                address = formatAddress(payload, offset)
            }
        }

        return address?.let { Target(it, findName(payload, 0)) }
    }

    private fun findName(payload: ByteArray, start: Int): String? {
        var name: String? = null

        forEachStructure(payload, start) { type, offset, length ->
            if ((type == TYPE_COMPLETE_NAME || (type == TYPE_SHORT_NAME && name == null)) && length > 0) {
                name = String(payload, offset, length, Charsets.UTF_8).trim()
            }
        }

        return name?.takeIf { it.isNotEmpty() }
    }

    private inline fun forEachStructure(payload: ByteArray, start: Int, block: (type: Int, offset: Int, length: Int) -> Unit) {
        var index = start

        while (index < payload.size) {
            val size = payload[index].toInt() and 0xFF

            if (size == 0) {
                break
            }

            val type = payload[index + 1].toInt() and 0xFF
            val dataLength = size - 1

            if (index + 2 + dataLength > payload.size) {
                break
            }

            block(type, index + 2, dataLength)
            index += size + 1
        }
    }

    private fun formatAddress(payload: ByteArray, offset: Int): String =
        (offset + 5 downTo offset).joinToString(":") { "%02X".format(payload[it].toInt() and 0xFF) }
}
