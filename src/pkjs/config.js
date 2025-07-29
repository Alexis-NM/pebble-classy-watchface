module.exports = [
  {
    type: "heading",
    defaultValue: "Réglages Classy",
  },
  {
    type: "section",
    items: [
      {
        type: "toggle",
        messageKey: "SecondTick",
        label: "Afficher les secondes",
        defaultValue: true,
      },
      {
        type: "toggle",
        messageKey: "InvertColors",
        label: "Inverser les couleurs",
        defaultValue: false,
      },
    ],
  },
  {
    type: "submit",
    defaultValue: "Enregistrer",
  },
];
