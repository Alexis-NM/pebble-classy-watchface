module.exports = [
  {
    type: "heading",
    defaultValue: "Classy Settings",
  },
  {
    type: "section",
    items: [
      {
        type: "toggle",
        messageKey: "SecondTick",
        label: "Show seconds",
        defaultValue: true,
      },
      {
        type: "toggle",
        messageKey: "InvertColors",
        label: "Invert colors",
        defaultValue: false,
      },
    ],
  },
  {
    type: "submit",
    defaultValue: "Save",
  },
];
