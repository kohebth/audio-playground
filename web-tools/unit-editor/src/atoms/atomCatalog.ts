// Atom catalog derived from dsp_types.h — describes ports and config fields for every atom

export type FieldDef = { name: string; type: 'float' | 'int' | 'string' | 'enum'; options?: string[] };

export type AtomDef = {
  name: string;
  category: 'generation' | 'amplitude' | 'filter' | 'delay' | 'mix' | 'detect' | 'modulation' | 'nonlinear' | 'freq' | 'src' | 'interpolation';
  ins: string[];
  outs: string[];
  config: FieldDef[];
};

export const ATOM_CATALOG: AtomDef[] = [
  // ── generation ──────────────────────────────────────────────────────────
  { name: 'generation_dc',          category: 'generation',  ins: [],                             outs: ['signal'],  config: [{ name: 'value', type: 'float' }] },
  { name: 'generation_impulse',     category: 'generation',  ins: [],                             outs: ['signal'],  config: [{ name: 'interval', type: 'float' }, { name: 'sample_rate', type: 'float' }] },
  { name: 'generation_noise',       category: 'generation',  ins: [],                             outs: ['signal'],  config: [{ name: 'amplitude', type: 'float' }, { name: 'color', type: 'int' }] },
  { name: 'generation_oscillator',  category: 'generation',  ins: [],                             outs: ['signal'],  config: [{ name: 'frequency', type: 'float' }, { name: 'waveform', type: 'int' }, { name: 'phase_offset', type: 'float' }, { name: 'sample_rate', type: 'float' }] },
  { name: 'generation_lfo',         category: 'generation',  ins: [],                             outs: ['signal'],  config: [{ name: 'frequency', type: 'float' }, { name: 'waveform', type: 'enum', options: ['sine','triangle','square','sawtooth'] }, { name: 'depth', type: 'float' }, { name: 'phase_offset', type: 'float' }] },
  { name: 'generation_envelope',    category: 'generation',  ins: ['gate'],                       outs: ['signal'],  config: [{ name: 'attack', type: 'float' }, { name: 'decay', type: 'float' }, { name: 'sustain', type: 'float' }, { name: 'release', type: 'float' }, { name: 'sample_rate', type: 'float' }] },

  // ── amplitude ────────────────────────────────────────────────────────────
  { name: 'amplitude_add',          category: 'amplitude',   ins: ['signal_a', 'signal_b'],       outs: ['signal'],  config: [] },
  { name: 'amplitude_subtract',     category: 'amplitude',   ins: ['signal_a', 'signal_b'],       outs: ['signal'],  config: [] },
  { name: 'amplitude_multiply',     category: 'amplitude',   ins: ['signal_a', 'signal_b'],       outs: ['signal'],  config: [] },
  { name: 'amplitude_divide',       category: 'amplitude',   ins: ['numerator', 'denominator'],   outs: ['signal'],  config: [{ name: 'epsilon', type: 'float' }] },
  { name: 'amplitude_accumulate',   category: 'amplitude',   ins: ['signal'],                     outs: ['signal'],  config: [] },
  { name: 'amplitude_smooth',       category: 'amplitude',   ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'attack', type: 'float' }, { name: 'release', type: 'float' }, { name: 'sample_rate', type: 'float' }] },
  { name: 'amplitude_clip_hard',    category: 'amplitude',   ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'threshold', type: 'float' }] },
  { name: 'amplitude_clip_soft',    category: 'amplitude',   ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'threshold', type: 'float' }, { name: 'curve', type: 'int' }] },
  { name: 'amplitude_normalize',    category: 'amplitude',   ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'target_level', type: 'float' }, { name: 'mode', type: 'int' }] },

  // ── filter ───────────────────────────────────────────────────────────────
  { name: 'filter_allpass',         category: 'filter',      ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'delay_samples', type: 'int' }, { name: 'coefficient', type: 'float' }] },
  { name: 'filter_biquad',          category: 'filter',      ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'b0', type: 'float' }, { name: 'b1', type: 'float' }, { name: 'b2', type: 'float' }, { name: 'a1', type: 'float' }, { name: 'a2', type: 'float' }] },
  { name: 'filter_comb_fb',         category: 'filter',      ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'delay_samples', type: 'int' }, { name: 'coefficient', type: 'float' }] },
  { name: 'filter_comb_ff',         category: 'filter',      ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'delay_samples', type: 'int' }, { name: 'coefficient', type: 'float' }] },
  { name: 'filter_dc_block',        category: 'filter',      ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'coefficient', type: 'float' }] },
  { name: 'filter_differentiate',   category: 'filter',      ins: ['signal'],                     outs: ['signal'],  config: [] },
  { name: 'filter_integrate',       category: 'filter',      ins: ['signal'],                     outs: ['signal'],  config: [] },
  { name: 'filter_fir',             category: 'filter',      ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'kernel_size', type: 'int' }] },

  // ── delay ────────────────────────────────────────────────────────────────
  { name: 'delay_line',             category: 'delay',       ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'length', type: 'int' }] },
  { name: 'delay_fractional',       category: 'delay',       ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'delay_samples', type: 'float' }, { name: 'interpolation', type: 'int' }] },
  { name: 'delay_unit',             category: 'delay',       ins: ['signal'],                     outs: ['signal'],  config: [] },
  { name: 'delay_tap_feedback',     category: 'delay',       ins: ['buffer'],                     outs: ['signal'],  config: [{ name: 'coefficient', type: 'float' }] },
  { name: 'delay_tap_feedforward',  category: 'delay',       ins: ['buffer'],                     outs: ['signal'],  config: [{ name: 'coefficient', type: 'float' }] },

  // ── mix ──────────────────────────────────────────────────────────────────
  { name: 'mix_wet_dry',            category: 'mix',         ins: ['dry', 'wet'],                 outs: ['signal'],  config: [{ name: 'mix', type: 'float' }] },
  { name: 'mix_crossfade',          category: 'mix',         ins: ['signal_a', 'signal_b'],       outs: ['signal'],  config: [{ name: 'position', type: 'float' }] },
  { name: 'mix_encode_ms',          category: 'mix',         ins: ['left', 'right'],              outs: ['mid', 'side'],  config: [] },
  { name: 'mix_decode_ms',          category: 'mix',         ins: ['mid', 'side'],                outs: ['left', 'right'],  config: [] },
  { name: 'mix_pan_stereo',         category: 'mix',         ins: ['signal'],                     outs: ['left', 'right'],  config: [{ name: 'pan', type: 'float' }] },
  { name: 'mix_matrix',             category: 'mix',         ins: ['signal'],                     outs: ['signal'],  config: [] },

  // ── detect ───────────────────────────────────────────────────────────────
  { name: 'detect_envelope',        category: 'detect',      ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'attack', type: 'float' }, { name: 'release', type: 'float' }, { name: 'sample_rate', type: 'float' }] },
  { name: 'detect_peak',            category: 'detect',      ins: ['signal'],                     outs: ['signal'],  config: [] },
  { name: 'detect_rms',             category: 'detect',      ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'window_size', type: 'int' }] },
  { name: 'detect_threshold',       category: 'detect',      ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'threshold', type: 'float' }] },
  { name: 'detect_slope',           category: 'detect',      ins: ['signal'],                     outs: ['signal'],  config: [] },
  { name: 'detect_zero_crossing',   category: 'detect',      ins: ['signal'],                     outs: ['signal'],  config: [] },
  { name: 'detect_autocorrelate',   category: 'detect',      ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'max_lag', type: 'int' }] },

  // ── modulation ───────────────────────────────────────────────────────────
  { name: 'modulation_amplitude',   category: 'modulation',  ins: ['signal', 'modulator'],        outs: ['signal'],  config: [{ name: 'depth', type: 'float' }] },
  { name: 'modulation_frequency',   category: 'modulation',  ins: ['signal', 'modulator'],        outs: ['signal'],  config: [{ name: 'depth', type: 'float' }] },
  { name: 'modulation_phase',       category: 'modulation',  ins: ['signal', 'modulator'],        outs: ['signal'],  config: [{ name: 'depth', type: 'float' }] },
  { name: 'modulation_ring',        category: 'modulation',  ins: ['signal', 'carrier'],          outs: ['signal'],  config: [] },
  { name: 'modulation_scrub',       category: 'modulation',  ins: ['signal', 'position'],         outs: ['signal'],  config: [] },

  // ── nonlinear ────────────────────────────────────────────────────────────
  { name: 'nonlinear_waveshape',         category: 'nonlinear',   ins: ['signal'],              outs: ['signal'],  config: [{ name: 'drive', type: 'float' }, { name: 'mode', type: 'int' }] },
  { name: 'nonlinear_bitcrush',          category: 'nonlinear',   ins: ['signal'],              outs: ['signal'],  config: [{ name: 'bits', type: 'int' }] },
  { name: 'nonlinear_samplerate_reduce', category: 'nonlinear',   ins: ['signal'],              outs: ['signal'],  config: [{ name: 'factor', type: 'int' }] },

  // ── freq ─────────────────────────────────────────────────────────────────
  { name: 'freq_fft',               category: 'freq',        ins: ['signal'],                     outs: ['real', 'imag'],  config: [{ name: 'size', type: 'int' }] },
  { name: 'freq_ifft',              category: 'freq',        ins: ['real', 'imag'],               outs: ['signal'],  config: [{ name: 'size', type: 'int' }] },
  { name: 'freq_multiply',          category: 'freq',        ins: ['real_a', 'imag_a', 'real_b', 'imag_b'], outs: ['real', 'imag'],  config: [] },
  { name: 'freq_window',            category: 'freq',        ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'type', type: 'int' }, { name: 'size', type: 'int' }] },
  { name: 'freq_overlap_add',       category: 'freq',        ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'size', type: 'int' }, { name: 'hop', type: 'int' }] },
  { name: 'freq_overlap_save',      category: 'freq',        ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'size', type: 'int' }, { name: 'hop', type: 'int' }] },

  // ── src ──────────────────────────────────────────────────────────────────
  { name: 'src_upsample',           category: 'src',         ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'factor', type: 'int' }] },
  { name: 'src_downsample',         category: 'src',         ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'factor', type: 'int' }] },
  { name: 'src_antialias',          category: 'src',         ins: ['signal'],                     outs: ['signal'],  config: [] },
  { name: 'src_antiimage',          category: 'src',         ins: ['signal'],                     outs: ['signal'],  config: [] },
  { name: 'src_convert_format',     category: 'src',         ins: ['signal'],                     outs: ['signal'],  config: [{ name: 'format', type: 'int' }] },

  // ── interpolation ────────────────────────────────────────────────────────
  { name: 'interpolation_linear',   category: 'interpolation', ins: ['signal'],                   outs: ['signal'],  config: [{ name: 'position', type: 'float' }] },
  { name: 'interpolation_cubic',    category: 'interpolation', ins: ['signal'],                   outs: ['signal'],  config: [{ name: 'position', type: 'float' }] },
  { name: 'interpolation_lagrange', category: 'interpolation', ins: ['signal'],                   outs: ['signal'],  config: [{ name: 'position', type: 'float' }, { name: 'order', type: 'int' }] },
  { name: 'interpolation_sinc',     category: 'interpolation', ins: ['signal'],                   outs: ['signal'],  config: [{ name: 'position', type: 'float' }, { name: 'num_taps', type: 'int' }] },
];

export const ATOM_MAP = new Map<string, AtomDef>(ATOM_CATALOG.map(a => [a.name, a]));

export const CATEGORY_COLORS: Record<AtomDef['category'], string> = {
  generation:    '#f59e0b',
  amplitude:     '#10b981',
  filter:        '#3b82f6',
  delay:         '#8b5cf6',
  mix:           '#ec4899',
  detect:        '#06b6d4',
  modulation:    '#f97316',
  nonlinear:     '#ef4444',
  freq:          '#6366f1',
  src:           '#14b8a6',
  interpolation: '#84cc16',
};
