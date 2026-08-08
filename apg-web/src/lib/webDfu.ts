export const DfuState = {
  DFU_APP_IDLE: 0,
  DFU_APP_DETACH: 1,
  DFU_IDLE: 2,
  DFU_DNLOAD_SYNC: 3,
  DFU_DNBUSY: 4,
  DFU_DNLOAD_IDLE: 5,
  DFU_MANIFEST_SYNC: 6,
  DFU_MANIFEST: 7,
  DFU_MANIFEST_WAIT_RESET: 8,
  DFU_UPLOAD_IDLE: 9,
  DFU_ERROR: 10,
} as const;

export type DfuState = typeof DfuState[keyof typeof DfuState];

export const DfuRequest = {
  DFU_DETACH: 0,
  DFU_DNLOAD: 1,
  DFU_UPLOAD: 2,
  DFU_GETSTATUS: 3,
  DFU_CLRSTATUS: 4,
  DFU_GETSTATE: 5,
  DFU_ABORT: 6,
} as const;

export type DfuStatus = {
  status: number;
  pollTimeout: number;
  state: DfuState;
  stringIndex: number;
};

export interface USBDevice {
  vendorId: number;
  productId: number;
  manufacturerName?: string;
  productName?: string;
  serialNumber?: string;
  opened: boolean;
  open(): Promise<void>;
  close(): Promise<void>;
  claimInterface(interfaceNumber: number): Promise<void>;
  controlTransferIn(setup: USBControlTransferParameters, length: number): Promise<USBInTransferResult>;
  controlTransferOut(setup: USBControlTransferParameters, data?: BufferSource): Promise<USBOutTransferResult>;
  reset(): Promise<void>;
}

export interface USBControlTransferParameters {
  requestType: 'standard' | 'class' | 'vendor';
  recipient: 'device' | 'interface' | 'endpoint' | 'other';
  request: number;
  value: number;
  index: number;
}

export interface USBInTransferResult {
  data?: DataView;
  status: 'ok' | 'stall' | 'babble';
}

export interface USBOutTransferResult {
  bytesWritten: number;
  status: 'ok' | 'stall' | 'babble';
}

export class UsbNotSupportedError extends Error {
  constructor() {
    super('WebUSB is not supported in this browser');
    this.name = 'UsbNotSupportedError';
  }
}

export class DfuError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'DfuError';
  }
}

export const STM_DFU_VENDOR_ID = 0x0483;
export const COMMON_USB_FILTERS = [
  { vendorId: STM_DFU_VENDOR_ID }, // STMicroelectronics
  { vendorId: 0x1a86 }, // CH340 / USB Serial
  { vendorId: 0x10c4 }, // CP210x USB Serial
  { vendorId: 0x0403 }, // FTDI
];

export async function detectStm32Devices(): Promise<USBDevice> {
  const nav = navigator as unknown as { usb?: { requestDevice: (opt: unknown) => Promise<USBDevice> } };
  if (!nav.usb) {
    throw new UsbNotSupportedError();
  }
  return await nav.usb.requestDevice({ filters: COMMON_USB_FILTERS });
}

export class DfuDevice {
  public readonly device: USBDevice;
  private readonly interfaceNumber: number;

  constructor(device: USBDevice, interfaceNumber: number = 0) {
    this.device = device;
    this.interfaceNumber = interfaceNumber;
  }

  async connect(): Promise<void> {
    if (!this.device.opened) {
      await this.device.open();
    }
    await this.device.claimInterface(this.interfaceNumber);
  }

  async getStatus(): Promise<DfuStatus> {
    const result = await this.device.controlTransferIn(
      {
        requestType: 'class',
        recipient: 'interface',
        request: DfuRequest.DFU_GETSTATUS,
        value: 0,
        index: this.interfaceNumber,
      },
      6
    );

    if (result.status !== 'ok' || !result.data) {
      throw new DfuError('Failed to get DFU status');
    }

    const data = result.data;
    return {
      status: data.getUint8(0),
      pollTimeout: data.getUint8(1) | (data.getUint8(2) << 8) | (data.getUint8(3) << 16),
      state: data.getUint8(4) as DfuState,
      stringIndex: data.getUint8(5),
    };
  }

  async clearStatus(): Promise<void> {
    const result = await this.device.controlTransferOut({
      requestType: 'class',
      recipient: 'interface',
      request: DfuRequest.DFU_CLRSTATUS,
      value: 0,
      index: this.interfaceNumber,
    });
    if (result.status !== 'ok') {
      throw new DfuError('Failed to clear DFU status');
    }
  }

  async abort(): Promise<void> {
    const result = await this.device.controlTransferOut({
      requestType: 'class',
      recipient: 'interface',
      request: DfuRequest.DFU_ABORT,
      value: 0,
      index: this.interfaceNumber,
    });
    if (result.status !== 'ok') {
      throw new DfuError('Failed to abort DFU operation');
    }
  }

  async download(blockNum: number, data: BufferSource): Promise<void> {
    const result = await this.device.controlTransferOut(
      {
        requestType: 'class',
        recipient: 'interface',
        request: DfuRequest.DFU_DNLOAD,
        value: blockNum,
        index: this.interfaceNumber,
      },
      data
    );
    if (result.status !== 'ok') {
      throw new DfuError('Failed to download block');
    }
  }

  async reset(): Promise<void> {
    try {
      await this.device.reset();
    } catch {
      // Ignore reset errors, device might disconnect
    }
  }
}

export class DfuFlasher {
  private readonly device: DfuDevice;
  private readonly transferSize: number;

  constructor(device: DfuDevice, transferSize: number = 1024) {
    this.device = device;
    this.transferSize = transferSize;
  }

  async flash(binary: ArrayBuffer, onProgress: (sent: number, total: number) => void): Promise<void> {
    let status = await this.device.getStatus();
    
    if (status.state === DfuState.DFU_ERROR) {
      await this.device.clearStatus();
      status = await this.device.getStatus();
    }
    
    if (status.state !== DfuState.DFU_IDLE) {
      await this.device.abort();
      status = await this.device.getStatus();
    }
    
    if (status.state !== DfuState.DFU_IDLE) {
      throw new DfuError(`Device is not in idle state (current: ${status.state})`);
    }

    const dataArray = new Uint8Array(binary);
    const totalBytes = dataArray.length;
    let bytesSent = 0;
    let blockNum = 0;

    while (bytesSent < totalBytes) {
      const bytesLeft = totalBytes - bytesSent;
      const chunkLength = Math.min(bytesLeft, this.transferSize);
      const chunk = dataArray.subarray(bytesSent, bytesSent + chunkLength);

      await this.device.download(blockNum++, chunk);

      status = await this.device.getStatus();
      while (status.state === DfuState.DFU_DNBUSY) {
        await new Promise((resolve) => setTimeout(resolve, status.pollTimeout || 1));
        status = await this.device.getStatus();
      }

      if (status.state !== DfuState.DFU_DNLOAD_IDLE) {
        throw new DfuError(`Flash failed at block ${blockNum - 1}, state: ${status.state}`);
      }

      bytesSent += chunkLength;
      onProgress(bytesSent, totalBytes);
    }

    // Send zero length block to signal end
    await this.device.download(blockNum, new ArrayBuffer(0));

    status = await this.device.getStatus();
    while (status.state === DfuState.DFU_DNBUSY) {
      await new Promise((resolve) => setTimeout(resolve, status.pollTimeout || 1));
      status = await this.device.getStatus();
    }
    
    if (status.state === DfuState.DFU_MANIFEST_SYNC) {
      // Send one more GETSTATUS to trigger manifest
      try {
        await this.device.getStatus();
      } catch {
        // Device might disconnect during manifest
      }
    }
  }
}
