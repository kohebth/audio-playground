export const APG_FIRMWARE_MAGIC = 0x4150474D; // "APGM"

export type FirmwareHeader = {
  magic: number;
  version: number;
  imageSize: number;
  crc32: number;
  targetBoard: number;
};

export type FirmwareValidationResult = {
  valid: boolean;
  header: FirmwareHeader | null;
  error?: string;
};

export type PresetValidationResult = {
  valid: boolean;
  error?: string;
};

export const APG_FIRMWARE_HEADER_SIZE = 32;

export function parseFirmwareHeader(buffer: ArrayBuffer): FirmwareHeader | null {
  if (buffer.byteLength < APG_FIRMWARE_HEADER_SIZE) {
    return null;
  }
  
  const view = new DataView(buffer);
  const magic = view.getUint32(0, true);
  
  if (magic !== APG_FIRMWARE_MAGIC) {
    return null;
  }
  
  return {
    magic,
    version: view.getUint32(4, true),
    imageSize: view.getUint32(8, true),
    crc32: view.getUint32(12, true),
    targetBoard: view.getUint32(16, true),
  };
}

export function computeCrc32(data: Uint8Array): number {
  const poly = 0xEDB88320;
  let crc = 0xFFFFFFFF;
  
  for (let i = 0; i < data.length; i++) {
    crc ^= data[i];
    for (let j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >>> 1) ^ poly;
      } else {
        crc >>>= 1;
      }
    }
  }
  
  return (crc ^ 0xFFFFFFFF) >>> 0;
}

export function validateFirmwarePackage(buffer: ArrayBuffer): FirmwareValidationResult {
  const header = parseFirmwareHeader(buffer);
  
  if (!header) {
    return { valid: false, header: null, error: 'Invalid or missing firmware magic header' };
  }
  
  if (buffer.byteLength < APG_FIRMWARE_HEADER_SIZE + header.imageSize) {
    return { valid: false, header, error: 'Firmware image truncated or invalid size' };
  }
  
  const imageData = new Uint8Array(buffer, APG_FIRMWARE_HEADER_SIZE, header.imageSize);
  const calculatedCrc = computeCrc32(imageData);
  
  if (calculatedCrc !== header.crc32) {
    return { 
      valid: false, 
      header, 
      error: `CRC32 mismatch. Expected 0x${header.crc32.toString(16)}, got 0x${calculatedCrc.toString(16)}` 
    };
  }
  
  return { valid: true, header };
}

export function validateYamlPreset(content: string): PresetValidationResult {
  if (!content.includes('schema: apg.project.v2')) {
    return { valid: false, error: 'Missing required schema header (schema: apg.project.v2)' };
  }
  
  const encoder = new TextEncoder();
  const bytes = encoder.encode(content);
  if (bytes.length > 64 * 1024) { // arbitrary sensible limit, e.g. 64KB
    return { valid: false, error: 'Preset file too large (exceeds 64KB limit)' };
  }
  
  return { valid: true };
}
