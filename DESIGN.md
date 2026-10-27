# Device management 1.1

The administration page is a local maintenance panel; the fleet viewer is a
private PC inventory. Both open with the device's useful state, then actions.
Network reachability and application data freshness are separate concepts.

UI tokens: paper `#edf3f8`, panel `#ffffff`, text `#18324b`, action `#175e91`,
healthy `#237357`, attention `#b77a12`. Segoe UI is used locally, with Arial as
fallback. No network fonts or external scripts are loaded, including in recovery.
One blue spine identifies the panel; colored bottom edges identify state without
replacing written labels. An inventory row groups identity, health and version/
last contact. On narrow screens it becomes a single column. The admin form uses
two columns for paired networks and IP settings, then one column on phones.

The design uses concrete Italian action labels, visible focus, associated form
labels, 44px controls and text explanations for trial rollback and unverified
updates. Versions, errors and Wi-Fi names are inserted as text, not HTML.

Reviewed against the brief: there is no decorative hero, chart, theme switcher,
cloud dependency, provider-specific bot or second server on the device. Personal
addresses live in the private inventory, outside the reusable core repository.
