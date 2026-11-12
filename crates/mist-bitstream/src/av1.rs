//! AV1 bitstream, OBU (Open Bitstream Unit) parser, and LEB128 decoder.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum Av1ObuType {
    Unknown = 0,
    SequenceHeader = 1,
    TemporalDelimiter = 2,
    FrameHeader = 3,
    TileGroup = 4,
    Metadata = 5,
    Frame = 6,
    RedundantFrameHeader = 7,
    TileList = 8,
    Padding = 15,
}

impl From<u8> for Av1ObuType {
    fn from(v: u8) -> Self {
        match v {
            1 => Self::SequenceHeader,
            2 => Self::TemporalDelimiter,
            3 => Self::FrameHeader,
            4 => Self::TileGroup,
            5 => Self::Metadata,
            6 => Self::Frame,
            7 => Self::RedundantFrameHeader,
            8 => Self::TileList,
            15 => Self::Padding,
            _ => Self::Unknown,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Av1ObuUnit {
    pub obu_type: Av1ObuType,
    pub offset: usize,
    pub header_size: usize,
    pub payload_size: usize,
    pub total_size: usize,
    pub has_extension: bool,
    pub temporal_id: u8,
    pub spatial_id: u8,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct Av1SequenceMetadata {
    pub profile: u8,
    pub level: u8,
    pub width: u32,
    pub height: u32,
    pub bit_depth: u32,
    pub is_10_bit: bool,
    pub is_hdr: bool,
    pub is_valid: bool,
}

pub struct Av1Parser;

impl Av1Parser {
    pub fn read_leb128(data: &[u8], offset: &mut usize) -> Option<u64> {
        let mut value = 0u64;
        for i in 0..8 {
            if *offset >= data.len() {
                return None;
            }
            let byte = data[*offset];
            *offset += 1;
            value |= ((byte & 0x7F) as u64) << (i * 7);
            if (byte & 0x80) == 0 {
                return Some(value);
            }
        }
        None
    }

    pub fn write_leb128(mut value: u64, out: &mut Vec<u8>) -> usize {
        let start_len = out.len();
        loop {
            let mut byte = (value & 0x7F) as u8;
            value >>= 7;
            if value > 0 {
                byte |= 0x80;
            }
            out.push(byte);
            if value == 0 {
                break;
            }
        }
        out.len() - start_len
    }

    pub fn parse_obus(data: &[u8]) -> Vec<Av1ObuUnit> {
        let mut obus = Vec::new();
        if data.is_empty() {
            return obus;
        }

        let mut offset = 0;
        while offset < data.len() {
            let obu_start = offset;
            let header_byte = data[offset];
            offset += 1;

            // bit 7 must be 0 (obu_forbidden_bit)
            if (header_byte & 0x80) != 0 {
                break;
            }

            let obu_type_int = (header_byte >> 3) & 0x0F;
            let has_extension = (header_byte & 0x04) != 0;
            let has_size_field = (header_byte & 0x02) != 0;

            let mut temporal_id = 0u8;
            let mut spatial_id = 0u8;

            if has_extension {
                if offset >= data.len() {
                    break;
                }
                let ext_byte = data[offset];
                offset += 1;
                temporal_id = (ext_byte >> 5) & 0x07;
                spatial_id = (ext_byte >> 3) & 0x03;
            }

            let payload_size = if has_size_field {
                match Self::read_leb128(data, &mut offset) {
                    Some(val) => val as usize,
                    None => break,
                }
            } else {
                data.len().saturating_sub(offset)
            };

            let header_size = offset - obu_start;
            let total_size = header_size + payload_size;

            obus.push(Av1ObuUnit {
                obu_type: Av1ObuType::from(obu_type_int),
                offset: obu_start,
                header_size,
                payload_size,
                total_size,
                has_extension,
                temporal_id,
                spatial_id,
            });

            offset += payload_size;
        }

        obus
    }

    pub fn is_keyframe_access_unit(data: &[u8]) -> bool {
        let obus = Self::parse_obus(data);
        for obu in &obus {
            if obu.obu_type == Av1ObuType::SequenceHeader {
                return true;
            }
            if obu.obu_type == Av1ObuType::Frame || obu.obu_type == Av1ObuType::FrameHeader {
                let payload_offset = obu.offset + obu.header_size;
                if payload_offset < data.len() {
                    let first_byte = data[payload_offset];
                    let show_existing_frame = (first_byte & 0x80) != 0;
                    if !show_existing_frame {
                        let frame_type = (first_byte >> 5) & 0x03;
                        if frame_type == 0 {
                            return true;
                        }
                    }
                }
            }
        }
        false
    }

    pub fn parse_sequence_header(data: &[u8]) -> Av1SequenceMetadata {
        let mut meta = Av1SequenceMetadata::default();
        let obus = Self::parse_obus(data);
        for obu in &obus {
            if obu.obu_type == Av1ObuType::SequenceHeader {
                let payload_offset = obu.offset + obu.header_size;
                if payload_offset < data.len() && obu.payload_size >= 4 {
                    let b0 = data[payload_offset];
                    meta.profile = (b0 >> 5) & 0x07;
                    meta.is_10_bit = (b0 & 0x01) != 0 || meta.profile == 2;
                    meta.bit_depth = if meta.is_10_bit { 10 } else { 8 };
                    meta.is_hdr = meta.is_10_bit;
                    meta.level = if obu.payload_size > 1 {
                        (data[payload_offset + 1] >> 3) & 0x1F
                    } else {
                        0
                    };
                    meta.is_valid = true;
                    return meta;
                }
            }
        }
        meta
    }
}
