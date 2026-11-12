//! H.264 (AVC) bitstream, NAL unit, and SPS metadata parser.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum NalType {
    Unspecified = 0,
    NonIdrSlice = 1,
    SlicePartitionA = 2,
    SlicePartitionB = 3,
    SlicePartitionC = 4,
    IdrSlice = 5,
    Sei = 6,
    Sps = 7,
    Pps = 8,
    Aud = 9,
    EndOfSequence = 10,
    EndOfStream = 11,
    FillerData = 12,
    Unknown = 31,
}

impl From<u8> for NalType {
    fn from(v: u8) -> Self {
        match v {
            0 => Self::Unspecified,
            1 => Self::NonIdrSlice,
            2 => Self::SlicePartitionA,
            3 => Self::SlicePartitionB,
            4 => Self::SlicePartitionC,
            5 => Self::IdrSlice,
            6 => Self::Sei,
            7 => Self::Sps,
            8 => Self::Pps,
            9 => Self::Aud,
            10 => Self::EndOfSequence,
            11 => Self::EndOfStream,
            12 => Self::FillerData,
            _ => Self::Unknown,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct NalUnit {
    pub nal_type: NalType,
    pub start_code_size: usize,
    pub offset: usize,
    pub header_offset: usize,
    pub payload_size: usize,
    pub total_size: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct SpsMetadata {
    pub profile_idc: u32,
    pub level_idc: u32,
    pub width: u32,
    pub height: u32,
    pub is_valid: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct AccessUnit {
    pub data: Vec<u8>,
    pub is_keyframe: bool,
    pub width: u32,
    pub height: u32,
    pub sequence_number: u64,
    pub timestamp_ns: u64,
}

pub struct BitReader<'a> {
    data: &'a [u8],
    byte_pos: usize,
    bit_pos: u32,
}

impl<'a> BitReader<'a> {
    pub fn new(data: &'a [u8]) -> Self {
        Self {
            data,
            byte_pos: 0,
            bit_pos: 0,
        }
    }

    pub fn read_bit(&mut self) -> Option<u32> {
        if self.byte_pos >= self.data.len() {
            return None;
        }
        let bit = ((self.data[self.byte_pos] >> (7 - self.bit_pos)) & 1) as u32;
        self.bit_pos += 1;
        if self.bit_pos == 8 {
            self.bit_pos = 0;
            self.byte_pos += 1;
        }
        Some(bit)
    }

    pub fn read_bits(&mut self, num_bits: u32) -> Option<u32> {
        let mut out = 0u32;
        for _ in 0..num_bits {
            let bit = self.read_bit()?;
            out = (out << 1) | bit;
        }
        Some(out)
    }

    pub fn read_ue(&mut self) -> Option<u32> {
        let mut leading_zeros = 0u32;
        loop {
            let bit = self.read_bit()?;
            if bit == 1 {
                break;
            }
            leading_zeros += 1;
            if leading_zeros > 31 {
                return None;
            }
        }
        if leading_zeros == 0 {
            return Some(0);
        }
        let suffix = self.read_bits(leading_zeros)?;
        Some((1u32 << leading_zeros) - 1 + suffix)
    }
}

pub fn remove_emulation_prevention(src: &[u8]) -> Vec<u8> {
    let mut rbsp = Vec::with_capacity(src.len());
    let mut i = 0;
    while i < src.len() {
        if i + 2 < src.len() && src[i] == 0 && src[i + 1] == 0 && src[i + 2] == 3 {
            rbsp.push(0);
            rbsp.push(0);
            i += 3;
        } else {
            rbsp.push(src[i]);
            i += 1;
        }
    }
    rbsp
}

pub struct H264Parser;

impl H264Parser {
    pub fn parse_nal_units(data: &[u8]) -> Vec<NalUnit> {
        let mut units = Vec::new();
        if data.len() < 4 {
            return units;
        }

        let mut i = 0;
        let mut last_start_pos: Option<usize> = None;
        let mut last_start_code_size = 0;

        let is_start_code = |idx: usize| -> Option<usize> {
            if idx + 4 <= data.len()
                && data[idx] == 0
                && data[idx + 1] == 0
                && data[idx + 2] == 0
                && data[idx + 3] == 1
            {
                Some(4)
            } else if idx + 3 <= data.len()
                && data[idx] == 0
                && data[idx + 1] == 0
                && data[idx + 2] == 1
            {
                Some(3)
            } else {
                None
            }
        };

        while i + 3 <= data.len() {
            if let Some(sc_size) = is_start_code(i) {
                if let Some(last_pos) = last_start_pos {
                    let offset = last_pos;
                    let header_offset = offset + last_start_code_size;
                    let total_size = i - offset;
                    let payload_size = total_size - last_start_code_size;
                    let nal_type = if header_offset < data.len() {
                        NalType::from(data[header_offset] & 0x1F)
                    } else {
                        NalType::Unspecified
                    };

                    units.push(NalUnit {
                        nal_type,
                        start_code_size: last_start_code_size,
                        offset,
                        header_offset,
                        payload_size,
                        total_size,
                    });
                }
                last_start_pos = Some(i);
                last_start_code_size = sc_size;
                i += sc_size;
            } else {
                i += 1;
            }
        }

        if let Some(last_pos) = last_start_pos {
            if last_pos < data.len() {
                let offset = last_pos;
                let header_offset = offset + last_start_code_size;
                let total_size = data.len() - offset;
                let payload_size = total_size.saturating_sub(last_start_code_size);
                let nal_type = if header_offset < data.len() {
                    NalType::from(data[header_offset] & 0x1F)
                } else {
                    NalType::Unspecified
                };

                units.push(NalUnit {
                    nal_type,
                    start_code_size: last_start_code_size,
                    offset,
                    header_offset,
                    payload_size,
                    total_size,
                });
            }
        }

        units
    }

    pub fn is_keyframe_access_unit(data: &[u8]) -> bool {
        let nals = Self::parse_nal_units(data);
        nals.iter().any(|nal| nal.nal_type == NalType::IdrSlice)
    }

    pub fn parse_sps(sps_payload: &[u8]) -> SpsMetadata {
        let mut meta = SpsMetadata::default();
        if sps_payload.len() < 4 {
            return meta;
        }

        let rbsp = remove_emulation_prevention(sps_payload);
        if rbsp.len() < 4 {
            return meta;
        }

        let mut reader = BitReader::new(&rbsp);

        // NAL header (1 byte)
        if reader.read_bits(8).is_none() {
            return meta;
        }

        let profile_idc = match reader.read_bits(8) {
            Some(v) => v,
            None => return meta,
        };
        meta.profile_idc = profile_idc;

        // constraint_flags (8 bits)
        if reader.read_bits(8).is_none() {
            return meta;
        }

        let level_idc = match reader.read_bits(8) {
            Some(v) => v,
            None => return meta,
        };
        meta.level_idc = level_idc;

        // sps_id
        if reader.read_ue().is_none() {
            return meta;
        }

        if matches!(
            profile_idc,
            100 | 110 | 122 | 244 | 44 | 83 | 86 | 118 | 128
        ) {
            let chroma_format_idc = match reader.read_ue() {
                Some(v) => v,
                None => return meta,
            };
            if chroma_format_idc == 3 && reader.read_bit().is_none() {
                return meta;
            }
            // bit_depth_luma_minus8
            if reader.read_ue().is_none() {
                return meta;
            }
            // bit_depth_chroma_minus8
            if reader.read_ue().is_none() {
                return meta;
            }
            // qpprime_y_zero_transform_bypass_flag
            if reader.read_bit().is_none() {
                return meta;
            }
            // seq_scaling_matrix_present_flag
            let seq_scaling = match reader.read_bit() {
                Some(v) => v,
                None => return meta,
            };
            if seq_scaling == 1 {
                let count = if chroma_format_idc != 3 { 8 } else { 12 };
                for i in 0..count {
                    let seq_list_present = match reader.read_bit() {
                        Some(v) => v,
                        None => return meta,
                    };
                    if seq_list_present == 1 {
                        let size_of_scaling = if i < 6 { 16 } else { 64 };
                        let mut last_scale = 8u32;
                        let mut next_scale = 8u32;
                        for _ in 0..size_of_scaling {
                            if next_scale != 0 {
                                let delta_scale = match reader.read_ue() {
                                    Some(v) => v,
                                    None => return meta,
                                };
                                next_scale = (last_scale + delta_scale) & 0xFF;
                            }
                            last_scale = if next_scale == 0 {
                                last_scale
                            } else {
                                next_scale
                            };
                        }
                    }
                }
            }
        }

        // log2_max_frame_num_minus4
        if reader.read_ue().is_none() {
            return meta;
        }

        let pic_order_cnt_type = match reader.read_ue() {
            Some(v) => v,
            None => return meta,
        };
        if pic_order_cnt_type == 0 {
            if reader.read_ue().is_none() {
                return meta;
            }
        } else if pic_order_cnt_type == 1 {
            if reader.read_bit().is_none()
                || reader.read_ue().is_none()
                || reader.read_ue().is_none()
            {
                return meta;
            }
            let num_ref_frames = match reader.read_ue() {
                Some(v) => v,
                None => return meta,
            };
            for _ in 0..num_ref_frames {
                if reader.read_ue().is_none() {
                    return meta;
                }
            }
        }

        // max_num_ref_frames
        if reader.read_ue().is_none() {
            return meta;
        }
        // gaps_in_frame_num_value_allowed_flag
        if reader.read_bit().is_none() {
            return meta;
        }

        let pic_width_in_mbs_minus1 = match reader.read_ue() {
            Some(v) => v,
            None => return meta,
        };
        let pic_height_in_map_units_minus1 = match reader.read_ue() {
            Some(v) => v,
            None => return meta,
        };
        let frame_mbs_only_flag = match reader.read_bit() {
            Some(v) => v,
            None => return meta,
        };

        if frame_mbs_only_flag == 0 && reader.read_bit().is_none() {
            return meta;
        }

        // direct_8x8_inference_flag
        if reader.read_bit().is_none() {
            return meta;
        }

        let frame_cropping_flag = match reader.read_bit() {
            Some(v) => v,
            None => return meta,
        };

        let mut crop_left = 0u32;
        let mut crop_right = 0u32;
        let mut crop_top = 0u32;
        let mut crop_bottom = 0u32;

        if frame_cropping_flag == 1 {
            crop_left = match reader.read_ue() {
                Some(v) => v,
                None => return meta,
            };
            crop_right = match reader.read_ue() {
                Some(v) => v,
                None => return meta,
            };
            crop_top = match reader.read_ue() {
                Some(v) => v,
                None => return meta,
            };
            crop_bottom = match reader.read_ue() {
                Some(v) => v,
                None => return meta,
            };
        }

        meta.width = (pic_width_in_mbs_minus1 + 1) * 16 - (crop_left + crop_right) * 2;
        meta.height = ((2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16)
            - (crop_top + crop_bottom) * 2;
        meta.is_valid = meta.width > 0 && meta.height > 0;
        meta
    }

    pub fn extract_access_units(data: &[u8], target_fps: u32) -> Vec<AccessUnit> {
        let mut access_units = Vec::new();
        if data.is_empty() {
            return access_units;
        }

        let nals = Self::parse_nal_units(data);
        if nals.is_empty() {
            return access_units;
        }

        let frame_interval_ns = if target_fps > 0 {
            1_000_000_000u64 / target_fps as u64
        } else {
            16_666_666u64
        };

        let mut au_start_indices = Vec::new();

        for (idx, nal) in nals.iter().enumerate() {
            if nal.nal_type == NalType::Aud {
                au_start_indices.push(idx);
            } else if nal.nal_type == NalType::Sps
                && (au_start_indices.is_empty() || *au_start_indices.last().unwrap() != idx)
            {
                if idx == 0
                    || nals[idx - 1].nal_type == NalType::IdrSlice
                    || nals[idx - 1].nal_type == NalType::NonIdrSlice
                {
                    au_start_indices.push(idx);
                }
            } else if (nal.nal_type == NalType::IdrSlice || nal.nal_type == NalType::NonIdrSlice)
                && au_start_indices.is_empty()
            {
                au_start_indices.push(idx);
            }
        }

        if au_start_indices.is_empty() {
            au_start_indices.push(0);
        }

        let mut current_width = 1920u32;
        let mut current_height = 1080u32;

        let mut seq = 0u64;

        for i in 0..au_start_indices.len() {
            let start_nal_idx = au_start_indices[i];
            let end_nal_idx = if i + 1 < au_start_indices.len() {
                au_start_indices[i + 1]
            } else {
                nals.len()
            };

            let start_byte = nals[start_nal_idx].offset;
            let end_byte = if end_nal_idx < nals.len() {
                nals[end_nal_idx].offset
            } else {
                data.len()
            };

            let au_slice = &data[start_byte..end_byte];
            let is_keyframe = nals[start_nal_idx..end_nal_idx]
                .iter()
                .any(|n| n.nal_type == NalType::IdrSlice);

            for n in &nals[start_nal_idx..end_nal_idx] {
                if n.nal_type == NalType::Sps {
                    let sps_slice = &data[n.header_offset..n.header_offset + n.payload_size];
                    let sps_meta = Self::parse_sps(sps_slice);
                    if sps_meta.is_valid {
                        current_width = sps_meta.width;
                        current_height = sps_meta.height;
                    }
                }
            }

            seq += 1;
            access_units.push(AccessUnit {
                data: au_slice.to_vec(),
                is_keyframe,
                width: current_width,
                height: current_height,
                sequence_number: seq,
                timestamp_ns: seq * frame_interval_ns,
            });
        }

        access_units
    }
}
