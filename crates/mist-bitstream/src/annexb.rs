//! Annex-B stream container, offline reader, and Access Unit enumerator.

use crate::h264::{AccessUnit, H264Parser};
use mist_core::{MistError, Result};
use serde::{Deserialize, Serialize};
use std::path::Path;

#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct StreamInfo {
    pub width: u32,
    pub height: u32,
    pub target_fps: u32,
    pub total_access_units: usize,
    pub keyframe_access_units: usize,
    pub total_bytes: usize,
    pub estimated_duration_seconds: f64,
}

#[derive(Debug, Clone, Default)]
pub struct AnnexBReader {
    info: StreamInfo,
    access_units: Vec<AccessUnit>,
}

impl AnnexBReader {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn open_file(file_path: impl AsRef<Path>, target_fps: u32) -> Result<Self> {
        let bytes = std::fs::read(&file_path).map_err(|e| {
            MistError::Io(format!(
                "Failed to open H.264 file {}: {}",
                file_path.as_ref().display(),
                e
            ))
        })?;
        Self::open_memory(&bytes, target_fps)
    }

    pub fn open_memory(data: &[u8], target_fps: u32) -> Result<Self> {
        if data.is_empty() {
            return Err(MistError::InvalidArgument("Buffer is empty".into()));
        }

        let access_units = H264Parser::extract_access_units(data, target_fps);
        if access_units.is_empty() {
            return Err(MistError::Internal(
                "No valid H.264 access units found in bitstream".into(),
            ));
        }

        let mut keyframe_count = 0;
        let mut width = 1920;
        let mut height = 1080;

        for au in &access_units {
            if au.is_keyframe {
                keyframe_count += 1;
            }
            if au.width > 0 && au.height > 0 {
                width = au.width;
                height = au.height;
            }
        }

        let total_access_units = access_units.len();
        let estimated_duration_seconds = if target_fps > 0 {
            total_access_units as f64 / target_fps as f64
        } else {
            0.0
        };

        let info = StreamInfo {
            width,
            height,
            target_fps,
            total_access_units,
            keyframe_access_units: keyframe_count,
            total_bytes: data.len(),
            estimated_duration_seconds,
        };

        Ok(Self { info, access_units })
    }

    pub fn stream_info(&self) -> &StreamInfo {
        &self.info
    }

    pub fn access_units(&self) -> &[AccessUnit] {
        &self.access_units
    }

    pub fn len(&self) -> usize {
        self.access_units.len()
    }

    pub fn is_empty(&self) -> bool {
        self.access_units.is_empty()
    }

    pub fn get_access_unit(&self, index: usize) -> Option<&AccessUnit> {
        self.access_units.get(index)
    }
}
