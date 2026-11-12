//! HEVC (H.265) bitstream, NAL unit, and SPS metadata parser.

use crate::h264::{remove_emulation_prevention, BitReader};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum HevcNalType {
    TrailN = 0,
    TrailR = 1,
    TsaN = 2,
    TsaR = 3,
    StsaN = 4,
    StsaR = 5,
    RadlN = 6,
    RadlR = 7,
    RaslN = 8,
    RaslR = 9,
    BlaWLp = 16,
    BlaWRadl = 17,
    BlaNLp = 18,
    IdrWRadl = 19,
    IdrNLp = 20,
    CraNut = 21,
    Vps = 32,
    Sps = 33,
    Pps = 34,
    Aud = 35,
    Eos = 36,
    Eob = 37,
    Fd = 38,
    PrefixSei = 39,
    SuffixSei = 40,
    Unknown = 63,
}

impl From<u8> for HevcNalType {
    fn from(v: u8) -> Self {
        match v {
            0 => Self::TrailN,
            1 => Self::TrailR,
            2 => Self::TsaN,
            3 => Self::TsaR,
            4 => Self::StsaN,
            5 => Self::StsaR,
            6 => Self::RadlN,
            7 => Self::RadlR,
            8 => Self::RaslN,
            9 => Self::RaslR,
            16 => Self::BlaWLp,
            17 => Self::BlaWRadl,
            18 => Self::BlaNLp,
            19 => Self::IdrWRadl,
            20 => Self::IdrNLp,
            21 => Self::CraNut,
            32 => Self::Vps,
            33 => Self::Sps,
            34 => Self::Pps,
            35 => Self::Aud,
            36 => Self::Eos,
            37 => Self::Eob,
            38 => Self::Fd,
            39 => Self::PrefixSei,
            40 => Self::SuffixSei,
            _ => Self::Unknown,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct HevcNalUnit {
    pub nal_type: HevcNalType,
    pub nuh_layer_id: u8,
    pub nuh_temporal_id_plus1: u8,
    pub start_code_size: usize,
    pub offset: usize,
    pub header_offset: usize,
    pub payload_size: usize,
    pub total_size: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct HevcSpsMetadata {
    pub profile_idc: u32,
    pub level_idc: u32,
    pub width: u32,
    pub height: u32,
    pub is_valid: bool,
}

pub struct HevcParser;

impl HevcParser {
    pub fn parse_nal_units(data: &[u8]) -> Vec<HevcNalUnit> {
        let mut units = Vec::new();
        if data.len() < 5 {
            return units;
        }

        #[derive(Clone, Copy)]
        struct StartCodePos {
            offset: usize,
            size: usize,
        }
        let mut start_codes = Vec::new();

        let mut i = 0;
        while i + 2 < data.len() {
            if data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1 {
                if i > 0 && data[i - 1] == 0 {
                    start_codes.push(StartCodePos {
                        offset: i - 1,
                        size: 4,
                    });
                } else {
                    start_codes.push(StartCodePos { offset: i, size: 3 });
                }
                i += 3;
                continue;
            }
            i += 1;
        }

        if start_codes.is_empty() {
            return units;
        }

        for k in 0..start_codes.len() {
            let sc = start_codes[k];
            let next_offset = if k + 1 < start_codes.len() {
                start_codes[k + 1].offset
            } else {
                data.len()
            };

            let header_offset = sc.offset + sc.size;
            if header_offset + 2 > data.len() {
                continue;
            }

            let byte0 = data[header_offset];
            let byte1 = data[header_offset + 1];

            // HEVC NAL header: forbidden_zero_bit (1), nal_unit_type (6), nuh_layer_id (6), nuh_temporal_id_plus1 (3)
            let nal_type_val = (byte0 >> 1) & 0x3F;
            let nuh_layer_id = ((byte0 & 1) << 5) | ((byte1 >> 3) & 0x1F);
            let nuh_temporal_id_plus1 = byte1 & 0x07;

            let total_size = next_offset - sc.offset;
            let payload_size = total_size.saturating_sub(sc.size);

            units.push(HevcNalUnit {
                nal_type: HevcNalType::from(nal_type_val),
                nuh_layer_id,
                nuh_temporal_id_plus1,
                start_code_size: sc.size,
                offset: sc.offset,
                header_offset,
                payload_size,
                total_size,
            });
        }

        units
    }

    pub fn is_keyframe_access_unit(data: &[u8]) -> bool {
        let nals = Self::parse_nal_units(data);
        for nal in nals {
            let type_val = nal.nal_type as u8;
            if (16..=21).contains(&type_val) {
                return true;
            }
        }
        false
    }

    pub fn parse_sps(sps_payload: &[u8]) -> HevcSpsMetadata {
        let mut meta = HevcSpsMetadata::default();
        if sps_payload.len() < 4 {
            return meta;
        }

        let rbsp = remove_emulation_prevention(sps_payload);
        if rbsp.len() < 4 {
            return meta;
        }

        // Skip 2-byte NAL unit header
        let mut reader = BitReader::new(&rbsp[2..]);

        // sps_video_parameter_set_id (4 bits)
        if reader.read_bits(4).is_none() {
            return meta;
        }
        let max_sub_layers_minus1 = match reader.read_bits(3) {
            Some(v) => v,
            None => return meta,
        };
        // sps_temporal_id_nesting_flag (1 bit)
        if reader.read_bit().is_none() {
            return meta;
        }

        // Profile Tier Level
        if reader.read_bits(2).is_none() || reader.read_bit().is_none() {
            return meta;
        }
        let profile_idc = match reader.read_bits(5) {
            Some(v) => v,
            None => return meta,
        };
        meta.profile_idc = profile_idc;

        // Skip 80 bits (32 compatibility + 4 constraint + 44 reserved)
        for _ in 0..(32 + 4 + 44) {
            if reader.read_bit().is_none() {
                return meta;
            }
        }

        let level_idc = match reader.read_bits(8) {
            Some(v) => v,
            None => return meta,
        };
        meta.level_idc = level_idc;

        let mut sub_layer_profile_present = Vec::new();
        let mut sub_layer_level_present = Vec::new();
        for _ in 0..max_sub_layers_minus1 {
            sub_layer_profile_present.push(reader.read_bit().unwrap_or(0));
            sub_layer_level_present.push(reader.read_bit().unwrap_or(0));
        }

        if max_sub_layers_minus1 > 0 {
            for _ in 0..(2 * (8 - max_sub_layers_minus1)) {
                if reader.read_bit().is_none() {
                    return meta;
                }
            }
        }

        for i in 0..max_sub_layers_minus1 as usize {
            if sub_layer_profile_present[i] == 1 {
                for _ in 0..(2 + 1 + 5 + 32 + 4 + 44) {
                    if reader.read_bit().is_none() {
                        return meta;
                    }
                }
            }
            if sub_layer_level_present[i] == 1 {
                for _ in 0..8 {
                    if reader.read_bit().is_none() {
                        return meta;
                    }
                }
            }
        }

        // sps_seq_parameter_set_id
        if reader.read_ue().is_none() {
            return meta;
        }
        let chroma_format_idc = match reader.read_ue() {
            Some(v) => v,
            None => return meta,
        };
        if chroma_format_idc == 3 && reader.read_bit().is_none() {
            return meta;
        }

        let pic_width_in_luma = match reader.read_ue() {
            Some(v) => v,
            None => return meta,
        };
        let pic_height_in_luma = match reader.read_ue() {
            Some(v) => v,
            None => return meta,
        };

        let conformance_window_flag = match reader.read_bit() {
            Some(v) => v,
            None => return meta,
        };

        let mut crop_left = 0u32;
        let mut crop_right = 0u32;
        let mut crop_top = 0u32;
        let mut crop_bottom = 0u32;

        if conformance_window_flag == 1 {
            crop_left = reader.read_ue().unwrap_or(0);
            crop_right = reader.read_ue().unwrap_or(0);
            crop_top = reader.read_ue().unwrap_or(0);
            crop_bottom = reader.read_ue().unwrap_or(0);
        }

        let sub_width_c = if chroma_format_idc == 1 || chroma_format_idc == 2 {
            2
        } else {
            1
        };
        let sub_height_c = if chroma_format_idc == 1 { 2 } else { 1 };

        meta.width = pic_width_in_luma.saturating_sub((crop_left + crop_right) * sub_width_c);
        meta.height = pic_height_in_luma.saturating_sub((crop_top + crop_bottom) * sub_height_c);
        meta.is_valid = meta.width > 0 && meta.height > 0;
        meta
    }
}
