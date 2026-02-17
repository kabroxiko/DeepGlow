/**
 * CommonModal - A parametric modal dialog for confirmation or alerts.
 *
 * Props:
 * - open: boolean (show/hide modal)
 * - title: string (modal title)
 * - description: string (modal body text)
 * - actions: array of { label, onClick, className, disabled } for buttons
 * - onClose: function (called when modal is dismissed)
 * - children: optional (for custom content)
 */
export function CommonModal({ open, title, description, actions = [], onClose, children }) {
  if (!open) return null;
  return (
    <dialog className="modal-overlay" open aria-modal="true">
      {/* Backdrop button for closing modal on click */}
      <button
        type="button"
        aria-label="Close modal"
        tabIndex={0}
        style={{
          position: "fixed",
          inset: 0,
          width: "100vw",
          height: "100vh",
          background: "transparent",
          border: 0,
          padding: 0,
          margin: 0,
          zIndex: 1,
          cursor: "pointer",
        }}
        onClick={onClose}
      />
      <div
        className="modal-confirm"
        style={{ position: "relative", zIndex: 2 }}
      >
        {title && <div className="modal-title">{title}</div>}
        {description && <div className="modal-desc">{description}</div>}
        {children}
        <div className="modal-actions">
          {actions.map(({ label, onClick, className = "btn", disabled = false }, idx) => (
            <button
              key={label + String(idx)}
              className={className}
              onClick={onClick}
              disabled={disabled}
            >
              {label}
            </button>
          ))}
        </div>
      </div>
    </dialog>
  );
}
