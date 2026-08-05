import { useEffect, useState } from 'react';

type Props = {
  open: boolean;
  onClose: () => void;
};

const steps = [
  {
    eyebrow: 'Your board',
    title: 'Build from left to right',
    body: 'The signal starts at Input, passes through each effect, and leaves at Output. Add an effect from the library to begin.',
  },
  {
    eyebrow: 'Hear it live',
    title: 'Preview your sound',
    body: 'Press Play with a microphone, audio interface, or mono audio file. The same transport stays available in both views.',
  },
  {
    eyebrow: 'Shape the sound',
    title: 'Turn the controls on each pedal',
    body: 'Drag a knob for immediate feedback. Tap the pedal footer to bypass it without removing it from the board.',
  },
  {
    eyebrow: 'Save a moment',
    title: 'Capture the whole board as a scene',
    body: 'Scenes remember every control and bypass state. Your work is also saved locally as you edit.',
  },
] as const;

export function GuidedTour({ open, onClose }: Props) {
  const [step, setStep] = useState(0);

  useEffect(() => {
    if (open) setStep(0);
  }, [open]);

  if (!open) return null;
  const current = steps[step];

  return (
    <div className="tour-backdrop" role="presentation">
      <section className="tour-card" aria-label="Audio Playground guided tour" aria-live="polite">
        <div className="tour-card__progress" aria-label={`Step ${step + 1} of ${steps.length}`}>
          {steps.map((_, index) => <i className={index <= step ? 'active' : ''} key={index} />)}
        </div>
        <button className="tour-card__close" onClick={onClose} type="button" aria-label="Close tour">×</button>
        <span className="tour-card__eyebrow">{current.eyebrow}</span>
        <h2>{current.title}</h2>
        <p>{current.body}</p>
        <footer>
          <button className="btn btn--ghost" onClick={onClose} type="button">Skip tour</button>
          <span>{step + 1} / {steps.length}</span>
          <button
            className="btn btn--primary"
            onClick={() => step === steps.length - 1 ? onClose() : setStep(value => value + 1)}
            type="button"
          >
            {step === steps.length - 1 ? 'Start creating' : 'Next'}
          </button>
        </footer>
      </section>
    </div>
  );
}
