export function AppLogo() {
  return (
    <img
      alt=""
      aria-hidden="true"
      className="app-logo"
      draggable={false}
      src={`${import.meta.env.BASE_URL}icon.svg`}
    />
  );
}
