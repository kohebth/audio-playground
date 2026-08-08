import { useState } from 'react';
import type { WorkspaceFile } from '../lib/backendSamples';
import { detectStm32Devices, DfuDevice, DfuFlasher, UsbNotSupportedError, type USBDevice } from '../lib/webDfu';
import { validateFirmwarePackage, validateYamlPreset } from '../lib/firmwarePackage';

type Props = {
  workspaceFiles: WorkspaceFile[];
  onClose: () => void;
  open: boolean;
};

export type UsbPortInfo = {
  vendorId: string;
  productId: string;
  manufacturerName?: string;
  productName?: string;
  serialNumber?: string;
  connected: boolean;
  deviceObject?: USBDevice;
};

export function FlashHardwareModal({ workspaceFiles, onClose, open }: Props) {
  const [targetDevice, setTargetDevice] = useState('STM32F729 (Cortex-M7)');
  const [flashType, setFlashType] = useState<'preset' | 'firmware'>('preset');
  const [selectedYamlPath, setSelectedYamlPath] = useState<string>('');
  const [customFirmwareFile, setCustomFirmwareFile] = useState<File | null>(null);
  
  // Real WebUSB / WebSerial USB Port Detection state
  const [usbPort, setUsbPort] = useState<UsbPortInfo | null>(null);
  const [connectingUsb, setConnectingUsb] = useState(false);
  const [usbError, setUsbError] = useState<string | null>(null);

  const [flashing, setFlashing] = useState(false);
  const [progress, setProgress] = useState(0);
  const [statusMessage, setStatusMessage] = useState<string | null>(null);
  const [flashLog, setFlashLog] = useState<string[]>([]);

  const handleScanWebUsb = async () => {
    setConnectingUsb(true);
    setUsbError(null);
    try {
      const device = await detectStm32Devices();
      await device.open();
      setUsbPort({
        vendorId: `0x${device.vendorId.toString(16).padStart(4, '0').toUpperCase()}`,
        productId: `0x${device.productId.toString(16).padStart(4, '0').toUpperCase()}`,
        manufacturerName: device.manufacturerName || 'STMicroelectronics',
        productName: device.productName || 'STM32 ST-Link / DFU Interface',
        serialNumber: device.serialNumber || '0001A03F9B2C',
        connected: true,
        deviceObject: device,
      });
      setFlashLog(prev => [...prev, `[USB] Connected to device: ${device.productName || 'STM32 Board'} (${device.vendorId.toString(16)}:${device.productId.toString(16)})`]);
    } catch (err: unknown) {
      if (err instanceof UsbNotSupportedError) {
        setUsbError('WebUSB is not supported in this browser. Please use Chrome or Edge.');
        setFlashLog(prev => [...prev, '[USB Error] WebUSB not supported.']);
      } else {
        const errMsg = err instanceof Error ? err.message : 'Connection cancelled by user';
        setUsbError(errMsg);
        setFlashLog(prev => [...prev, `[USB Error] ${errMsg}`]);
      }
    } finally {
      setConnectingUsb(false);
    }
  };

  if (!open) return null;

  const yamlFiles = workspaceFiles.filter(f => f.path.endsWith('.yaml') || f.path.endsWith('.yml'));
  const currentYamlContent = workspaceFiles.find(f => f.path === (selectedYamlPath || yamlFiles[0]?.path))?.content ?? '';

  const readFileAsArrayBuffer = (file: File): Promise<ArrayBuffer> => {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(reader.result as ArrayBuffer);
      reader.onerror = () => reject(new Error('Failed to read file'));
      reader.readAsArrayBuffer(file);
    });
  };

  const handleFlash = async () => {
    if (!usbPort || !usbPort.deviceObject) {
      setUsbError('No USB port connected! Click "Detect USB Board" first.');
      return;
    }

    setFlashing(true);
    setProgress(5);
    setFlashLog([`[Init] Target: ${targetDevice}`, `[Init] Connection: USB Serial/DFU (${usbPort.vendorId}:${usbPort.productId})`]);
    setStatusMessage('Initiating ST-Link / DFU Bootloader Handshake...');

    try {
      let flashBuffer: ArrayBuffer;

      if (flashType === 'preset') {
        setStatusMessage(`Validating YAML Preset Patch: ${selectedYamlPath || yamlFiles[0]?.path || 'active.project.v2.yaml'}...`);
        const presetValidation = validateYamlPreset(currentYamlContent);
        if (!presetValidation.valid) {
          throw new Error(`Preset validation failed: ${presetValidation.error}`);
        }
        const encoder = new TextEncoder();
        flashBuffer = encoder.encode(currentYamlContent).buffer;
        setFlashLog(prev => [...prev, `[Validation] YAML Preset valid. Size: ${flashBuffer.byteLength} bytes.`]);
      } else {
        if (!customFirmwareFile) {
          throw new Error('Please select a firmware file first.');
        }
        setStatusMessage(`Reading & Validating Firmware Binary ${customFirmwareFile.name}...`);
        const buffer = await readFileAsArrayBuffer(customFirmwareFile);
        const fwValidation = validateFirmwarePackage(buffer);
        if (!fwValidation.valid) {
          throw new Error(`Firmware validation failed: ${fwValidation.error}`);
        }
        flashBuffer = buffer;
        setFlashLog(prev => [
          ...prev, 
          `[Validation] Firmware header OK (Magic: 0x${fwValidation.header!.magic.toString(16)}, Size: ${fwValidation.header!.imageSize} bytes, CRC32: 0x${fwValidation.header!.crc32.toString(16)}).`
        ]);
      }

      const dfuDevice = new DfuDevice(usbPort.deviceObject);
      await dfuDevice.connect();

      const flasher = new DfuFlasher(dfuDevice);
      
      setStatusMessage('Flashing to device...');
      setFlashLog(prev => [...prev, '[DFU] Erasing and flashing memory...']);
      
      await flasher.flash(flashBuffer, (sent, total) => {
        const percent = Math.round((sent / total) * 100);
        setProgress(Math.max(10, percent - 5)); // Keep a little buffer for finalization
      });

      setFlashLog(prev => [...prev, '[DFU] Flashing completed successfully.']);
      
      setProgress(95);
      setStatusMessage('Rebooting device...');
      await dfuDevice.reset();
      
      setProgress(100);
      setStatusMessage(flashType === 'preset' ? 'Preset patch flashed and live-reloaded!' : 'Firmware binary updated successfully!');
      setFlashLog(prev => [...prev, '[System] Target reset executed.']);
    } catch (err: unknown) {
      const errMsg = err instanceof Error ? err.message : 'Operation failed';
      setStatusMessage(`Flash Error: ${errMsg}`);
      setFlashLog(prev => [...prev, `[Fatal Error] ${errMsg}`]);
    } finally {
      setFlashing(false);
    }
  };

  return (
    <div className="modal-backdrop" role="dialog" aria-label="Flash Hardware">
      <div className="modal-card" style={{ maxWidth: '640px', width: '100%' }}>
        <div className="modal-card__header">
          <h3>
            <i className="fa-solid fa-microchip" style={{ marginRight: '8px', color: 'var(--color-primary-400)' }} />
            STM32 Hardware Flasher & Preset Loader
          </h3>
          <button className="btn btn--ghost" onClick={onClose} disabled={flashing} type="button">
            <i className="fa-solid fa-xmark" />
          </button>
        </div>

        <div className="modal-card__body" style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
          {/* USB Connection Card */}
          <div style={{ background: 'var(--color-surface-2)', border: '1px solid var(--color-border)', padding: '14px', borderRadius: '8px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <strong style={{ fontSize: '13px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <i className={`fa-solid fa-plug ${usbPort ? 'color-success' : ''}`} />
                  USB Hardware Port Status
                </strong>
                <span style={{ fontSize: '12px', color: 'var(--color-text-subtle)', display: 'block', marginTop: '2px' }}>
                  {usbPort
                    ? `${usbPort.productName} (${usbPort.vendorId}:${usbPort.productId}) S/N: ${usbPort.serialNumber}`
                    : 'No STM32 USB DFU / Serial port connected'}
                </span>
              </div>
              <button
                className={`btn ${usbPort ? 'btn--ghost' : 'btn--primary'}`}
                onClick={handleScanWebUsb}
                disabled={connectingUsb || flashing}
                type="button"
                style={{ fontSize: '12px' }}
              >
                <i className={`fa-solid ${connectingUsb ? 'fa-spinner fa-spin' : 'fa-usb'}`} style={{ marginRight: '6px' }} />
                {connectingUsb ? 'Scanning...' : usbPort ? 'Change Port' : 'Detect USB Board'}
              </button>
            </div>
            {usbError ? (
              <div style={{ marginTop: '8px', fontSize: '11px', color: 'var(--color-danger)', background: 'rgba(255,0,0,0.1)', padding: '6px 10px', borderRadius: '4px' }}>
                <i className="fa-solid fa-triangle-exclamation" style={{ marginRight: '4px' }} />
                {usbError}
              </div>
            ) : null}
          </div>

          {/* Select Hardware Target Device */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            <div>
              <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--color-text-subtle)', textTransform: 'uppercase' }}>
                Target Board & Memory
              </label>
              <select
                className="select"
                value={targetDevice}
                onChange={e => setTargetDevice(e.target.value)}
                disabled={flashing}
                style={{ width: '100%', marginTop: '4px', fontSize: '12px' }}
              >
                <option value="STM32F729 (Cortex-M7)">STM32F729 @ 216MHz (8MB SDRAM)</option>
                <option value="STM32H743 (Cortex-M7)">STM32H743 @ 480MHz (16MB SDRAM)</option>
                <option value="Custom Cortex-M7 Board">Custom STM32F7 / M7 Board</option>
              </select>
            </div>

            <div>
              <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--color-text-subtle)', textTransform: 'uppercase' }}>
                Flash Mode Selection
              </label>
              <select
                className="select"
                value={flashType}
                onChange={e => setFlashType(e.target.value as 'preset' | 'firmware')}
                disabled={flashing}
                style={{ width: '100%', marginTop: '4px', fontSize: '12px' }}
              >
                <option value="preset">YAML Preset Patch Update</option>
                <option value="firmware">Full Firmware Binary Update (.bin)</option>
              </select>
            </div>
          </div>

          {/* Payload Configuration */}
          {flashType === 'preset' ? (
            <div style={{ background: 'var(--color-surface-1)', border: '1px solid var(--color-border)', padding: '12px', borderRadius: '6px' }}>
              <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--color-text-subtle)', textTransform: 'uppercase' }}>
                Select Workspace Preset File
              </label>
              <select
                className="select"
                value={selectedYamlPath || yamlFiles[0]?.path}
                onChange={e => setSelectedYamlPath(e.target.value)}
                disabled={flashing}
                style={{ width: '100%', marginTop: '4px', marginBottom: '8px', fontSize: '12px' }}
              >
                {yamlFiles.map(f => (
                  <option key={f.path} value={f.path}>
                    {f.path} ({f.content.length} bytes)
                  </option>
                ))}
              </select>

              <div style={{ fontSize: '11px', color: 'var(--color-text-subtle)' }}>Preview Payload:</div>
              <pre style={{ maxHeight: '110px', overflowY: 'auto', background: 'var(--color-surface-2)', padding: '8px', borderRadius: '4px', fontSize: '11px', marginTop: '4px' }}>
                {currentYamlContent || '# No YAML preset selected'}
              </pre>
            </div>
          ) : (
            <div style={{ background: 'var(--color-surface-1)', border: '1px solid var(--color-border)', padding: '12px', borderRadius: '6px' }}>
              <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--color-text-subtle)', textTransform: 'uppercase' }}>
                Firmware Binary File (.bin)
              </label>
              <div style={{ display: 'flex', gap: '8px', marginTop: '4px', alignItems: 'center' }}>
                <input
                  type="file"
                  accept=".bin,.hex,.elf"
                  onChange={e => setCustomFirmwareFile(e.target.files?.[0] || null)}
                  disabled={flashing}
                  style={{ fontSize: '12px' }}
                />
              </div>
              <p style={{ marginTop: '6px', fontSize: '11px', color: 'var(--color-text-subtle)' }}>
                {customFirmwareFile
                  ? `Selected: ${customFirmwareFile.name} (${(customFirmwareFile.size / 1024).toFixed(1)} KB)`
                  : 'Please select a valid APG firmware binary.'}
              </p>
            </div>
          )}

          {/* Flash Log Terminal Output */}
          {flashLog.length > 0 ? (
            <div style={{ background: '#0d1117', color: '#3fb950', border: '1px solid #30363d', borderRadius: '6px', padding: '10px', fontFamily: 'monospace', fontSize: '11px', maxHeight: '120px', overflowY: 'auto' }}>
              {flashLog.map((line, idx) => (
                <div key={idx}>{line}</div>
              ))}
            </div>
          ) : null}

          {/* Progress Indicator */}
          {flashing || progress > 0 ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px' }}>
                <span>{statusMessage}</span>
                <strong>{progress}%</strong>
              </div>
              <div style={{ height: '8px', background: 'var(--color-surface-3)', borderRadius: '4px', overflow: 'hidden' }}>
                <div style={{ width: `${progress}%`, height: '100%', background: 'var(--color-primary-500)', transition: 'width 0.3s' }} />
              </div>
            </div>
          ) : null}
        </div>

        <div className="modal-card__footer" style={{ display: 'flex', justifyContent: 'flex-end', gap: '8px' }}>
          <button className="btn btn--ghost" onClick={onClose} disabled={flashing} type="button">
            Cancel
          </button>
          <button className="btn btn--primary" onClick={handleFlash} disabled={flashing} type="button">
            <i className={`fa-solid ${flashing ? 'fa-spinner fa-spin' : 'fa-bolt'}`} style={{ marginRight: '6px' }} />
            {flashing ? 'Flashing Target...' : flashType === 'preset' ? 'Flash Preset Patch' : 'Flash Firmware Binary'}
          </button>
        </div>
      </div>
    </div>
  );
}
