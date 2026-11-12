//! RFC 5109 / RFC 8627 XOR Forward Error Correction (FEC) Encoder and Decoder.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct FecPacket {
    pub base_sequence_number: u16,
    pub mask: u16,
    pub length_recovery: u16,
    pub payload: Vec<u8>,
}

#[derive(Debug, Clone)]
struct MediaEntry {
    seq: u16,
    data: Vec<u8>,
}

pub struct FecEncoder {
    step_size: u32,
    current_group: Vec<MediaEntry>,
}

impl Default for FecEncoder {
    fn default() -> Self {
        Self::new(5)
    }
}

impl FecEncoder {
    pub fn new(step_size: u32) -> Self {
        Self {
            step_size: step_size.max(1),
            current_group: Vec::new(),
        }
    }

    pub fn set_step_size(&mut self, step_size: u32) {
        self.step_size = step_size.max(1);
    }

    pub fn step_size(&self) -> u32 {
        self.step_size
    }

    pub fn add_media_packet(&mut self, seq_number: u16, payload: &[u8]) -> Option<FecPacket> {
        self.current_group.push(MediaEntry {
            seq: seq_number,
            data: payload.to_vec(),
        });

        if self.current_group.len() >= self.step_size as usize {
            let packets = self.generate_pending_fec_packets();
            packets.into_iter().next()
        } else {
            None
        }
    }

    pub fn generate_pending_fec_packets(&mut self) -> Vec<FecPacket> {
        if self.current_group.is_empty() {
            return Vec::new();
        }

        let base_seq = self.current_group[0].seq;
        let mut mask = 0u16;
        let mut length_recovery = 0u16;
        let mut max_len = 0usize;

        for entry in &self.current_group {
            max_len = max_len.max(entry.data.len());
            let diff = entry.seq.wrapping_sub(base_seq) as i16;
            if (0..16).contains(&diff) {
                mask |= 1u16 << (diff as u16);
            }
            length_recovery ^= entry.data.len() as u16;
        }

        let mut fec_payload = vec![0u8; max_len];
        for entry in &self.current_group {
            for (b, &byte) in entry.data.iter().enumerate() {
                fec_payload[b] ^= byte;
            }
        }

        let fec = FecPacket {
            base_sequence_number: base_seq,
            mask,
            length_recovery,
            payload: fec_payload,
        };

        self.current_group.clear();
        vec![fec]
    }

    pub fn reset(&mut self) {
        self.current_group.clear();
    }
}

#[derive(Default)]
pub struct FecDecoder {
    received_media: HashMap<u16, Vec<u8>>,
    pending_fec: Vec<FecPacket>,
    packets_recovered: u64,
}

impl FecDecoder {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn add_media_packet(&mut self, seq_number: u16, payload: &[u8]) {
        self.received_media.insert(seq_number, payload.to_vec());
    }

    pub fn add_fec_packet(&mut self, fec: FecPacket) {
        self.pending_fec.push(fec);
    }

    pub fn packets_recovered(&self) -> u64 {
        self.packets_recovered
    }

    pub fn try_recover_packets(&mut self) -> Vec<(u16, Vec<u8>)> {
        let mut recovered = Vec::new();
        let mut progress = true;

        while progress {
            progress = false;

            let mut i = 0;
            while i < self.pending_fec.len() {
                let fec = &self.pending_fec[i];
                let mut missing_seqs = Vec::new();
                let mut present_seqs = Vec::new();

                for bit in 0..16u16 {
                    if (fec.mask & (1 << bit)) != 0 {
                        let seq = fec.base_sequence_number.wrapping_add(bit);
                        if self.received_media.contains_key(&seq) {
                            present_seqs.push(seq);
                        } else {
                            missing_seqs.push(seq);
                        }
                    }
                }

                if missing_seqs.len() == 1 {
                    // Exactly one packet missing: perform XOR recovery
                    let target_seq = missing_seqs[0];

                    let mut recovered_len = fec.length_recovery;
                    for &s in &present_seqs {
                        recovered_len ^= self.received_media[&s].len() as u16;
                    }

                    let mut recovered_data = fec.payload.clone();
                    for &s in &present_seqs {
                        let other = &self.received_media[&s];
                        for (idx, &byte) in other.iter().enumerate() {
                            if idx < recovered_data.len() {
                                recovered_data[idx] ^= byte;
                            }
                        }
                    }

                    recovered_data.truncate(recovered_len as usize);

                    self.received_media
                        .insert(target_seq, recovered_data.clone());
                    recovered.push((target_seq, recovered_data));
                    self.packets_recovered += 1;

                    self.pending_fec.remove(i);
                    progress = true;
                } else if missing_seqs.is_empty() {
                    // All packets already present, discard this FEC packet
                    self.pending_fec.remove(i);
                } else {
                    i += 1;
                }
            }
        }

        recovered
    }
}
