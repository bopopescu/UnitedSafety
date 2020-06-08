#pragma once
static ats::String gasCode = "\
[\
   {\
      \"code\" : \"G0001\",\
      \"description\" : \"Carbon Monoxide\"\
   },\
   {\
      \"code\" : \"G0002\",\
      \"description\" : \"Hydrogen Sulfide\"\
   },\
   {\
      \"code\" : \"G0003\",\
      \"description\" : \"Sulfur Dioxide\"\
   },\
   {\
      \"description\" : \"Nitrogen Dioxide\",\
      \"code\" : \"G0004\"\
   },\
   {\
      \"code\" : \"G0005\",\
      \"description\" : \"Chlorine\"\
   },\
   {\
      \"code\" : \"G0006\",\
      \"description\" : \"Chlorine Dioxide\"\
   },\
   {\
      \"description\" : \"Hydrogen Cyanide\",\
      \"code\" : \"G0007\"\
   },\
   {\
      \"description\" : \"Phosphine\",\
      \"code\" : \"G0008\"\
   },\
   {\
      \"code\" : \"G0009\",\
      \"description\" : \"Hydrogen\"\
   },\
   {\
      \"code\" : \"G0011\",\
      \"description\" : \"Carbon Dioxide\"\
   },\
   {\
      \"code\" : \"G0012\",\
      \"description\" : \"Nitric Oxide\"\
   },\
   {\
      \"description\" : \"Ammonia\",\
      \"code\" : \"G0013\"\
   },\
   {\
      \"description\" : \"Hydrogen Chloride\",\
      \"code\" : \"G0014\"\
   },\
   {\
      \"code\" : \"G0015\",\
      \"description\" : \"Ozone\"\
   },\
   {\
      \"description\" : \"Phosgene\",\
      \"code\" : \"G0016\"\
   },\
   {\
      \"description\" : \"Hydrogen Fluoride\",\
      \"code\" : \"G0017\"\
   },\
   {\
      \"code\" : \"G0020\",\
      \"description\" : \"Oxygen\"\
   },\
   {\
      \"description\" : \"Methane\",\
      \"code\" : \"G0021\"\
   },\
   {\
      \"code\" : \"G0022\",\
      \"description\" : \"Unidentified Combustible Gas\"\
   },\
   {\
      \"code\" : \"G0023\",\
      \"description\" : \"Hexane\"\
   },\
   {\
      \"code\" : \"G0026\",\
      \"description\" : \"Pentane\"\
   },\
   {\
      \"description\" : \"Propane\",\
      \"code\" : \"G0027\"\
   },\
   {\
      \"code\" : \"G0028\",\
      \"description\" : \"1,4-Butanediol\"\
   },\
   {\
      \"code\" : \"G0029\",\
      \"description\" : \"1,4-Dioxane\"\
   },\
   {\
      \"description\" : \"1,2,4-Trimethyl benzene\",\
      \"code\" : \"G0030\"\
   },\
   {\
      \"description\" : \"1,2,3-Trimethyl benzene\",\
      \"code\" : \"G0031\"\
   },\
   {\
      \"description\" : \"1,2-Dibromomethane\",\
      \"code\" : \"G0032\"\
   },\
   {\
      \"description\" : \"1,2-Dichlorobenzene\",\
      \"code\" : \"G0033\"\
   },\
   {\
      \"description\" : \"1,3,5-Trimethylbenzene\",\
      \"code\" : \"G0034\"\
   },\
   {\
      \"description\" : \"1-Butanol\",\
      \"code\" : \"G0035\"\
   },\
   {\
      \"code\" : \"G0036\",\
      \"description\" : \"1-Methoxy-2-propanol\"\
   },\
   {\
      \"code\" : \"G0037\",\
      \"description\" : \"1-Propanol\"\
   },\
   {\
      \"description\" : \"Methyl acetate\",\
      \"code\" : \"G0038\"\
   },\
   {\
      \"description\" : \"Methyl acrylate\",\
      \"code\" : \"G0039\"\
   },\
   {\
      \"code\" : \"G0040\",\
      \"description\" : \"Methyl acetoacetate\"\
   },\
   {\
      \"description\" : \"Methyl benzoate\",\
      \"code\" : \"G0041\"\
   },\
   {\
      \"code\" : \"G0042\",\
      \"description\" : \"Methyl methacrylate\"\
   },\
   {\
      \"code\" : \"G0043\",\
      \"description\" : \"2-Butanone\"\
   },\
   {\
      \"code\" : \"G0044\",\
      \"description\" : \"1-Methyl formamide\"\
   },\
   {\
      \"code\" : \"G0045\",\
      \"description\" : \"Methoxy ethanol\"\
   },\
   {\
      \"description\" : \"2-Pentanone\",\
      \"code\" : \"G0046\"\
   },\
   {\
      \"code\" : \"G0047\",\
      \"description\" : \"2-Picoline\"\
   },\
   {\
      \"code\" : \"G0048\",\
      \"description\" : \"2-Propanol\"\
   },\
   {\
      \"code\" : \"G0049\",\
      \"description\" : \"2-Methyl formamide\"\
   },\
   {\
      \"code\" : \"G0050\",\
      \"description\" : \"Dimethyl acetamide\"\
   },\
   {\
      \"description\" : \"3-Picoline\",\
      \"code\" : \"G0051\"\
   },\
   {\
      \"code\" : \"G0052\",\
      \"description\" : \"Diacetone alcohol\"\
   },\
   {\
      \"code\" : \"G0053\",\
      \"description\" : \"Acetaldehyde\"\
   },\
   {\
      \"code\" : \"G0054\",\
      \"description\" : \"Acetone\"\
   },\
   {\
      \"code\" : \"G0055\",\
      \"description\" : \"Acetophenone\"\
   },\
   {\
      \"code\" : \"G0056\",\
      \"description\" : \"Allyl alcohol\"\
   },\
   {\
      \"code\" : \"G0057\",\
      \"description\" : \"Ammonia\"\
   },\
   {\
      \"description\" : \"Amyl acetate\",\
      \"code\" : \"G0058\"\
   },\
   {\
      \"description\" : \"Benzene\",\
      \"code\" : \"G0059\"\
   },\
   {\
      \"description\" : \"Methyl Bromide\",\
      \"code\" : \"G0060\"\
   },\
   {\
      \"description\" : \"Butoxy ethanol\",\
      \"code\" : \"G0062\"\
   },\
   {\
      \"code\" : \"G0063\",\
      \"description\" : \"Butyl acetate\"\
   },\
   {\
      \"code\" : \"G0064\",\
      \"description\" : \"Tetrachloroethylene\"\
   },\
   {\
      \"code\" : \"G0065\",\
      \"description\" : \"1,1-Dichloroethane\"\
   },\
   {\
      \"code\" : \"G0066\",\
      \"description\" : \"Ethyl benzene\"\
   },\
   {\
      \"code\" : \"G0067\",\
      \"description\" : \"Trichloroethylene\"\
   },\
   {\
      \"description\" : \"Ethyl acetoacetate\",\
      \"code\" : \"G0068\"\
   },\
   {\
      \"description\" : \"Chorobenzene\",\
      \"code\" : \"G0069\"\
   },\
   {\
      \"description\" : \"Cumene\",\
      \"code\" : \"G0070\"\
   },\
   {\
      \"description\" : \"Cylclohexane\",\
      \"code\" : \"G0071\"\
   },\
   {\
      \"code\" : \"G0072\",\
      \"description\" : \"Cyclohexanone\"\
   },\
   {\
      \"description\" : \"Decane\",\
      \"code\" : \"G0073\"\
   },\
   {\
      \"code\" : \"G0074\",\
      \"description\" : \"Diethylamine\"\
   },\
   {\
      \"code\" : \"G0075\",\
      \"description\" : \"Dimethoxymethane\"\
   },\
   {\
      \"code\" : \"G0076\",\
      \"description\" : \"Epichlorohydrin\"\
   },\
   {\
      \"description\" : \"Ethanol\",\
      \"code\" : \"G0077\"\
   },\
   {\
      \"code\" : \"G0078\",\
      \"description\" : \"Ethylene glycol\"\
   },\
   {\
      \"description\" : \"Ethyl acetate\",\
      \"code\" : \"G0079\"\
   },\
   {\
      \"description\" : \"Ethylene\",\
      \"code\" : \"G0080\"\
   },\
   {\
      \"code\" : \"G0081\",\
      \"description\" : \"Ethylene oxide\"\
   },\
   {\
      \"code\" : \"G0082\",\
      \"description\" : \"Butyrolactone\"\
   },\
   {\
      \"code\" : \"G0083\",\
      \"description\" : \"Hydrogen Sulfide\"\
   },\
   {\
      \"description\" : \"Heptane\",\
      \"code\" : \"G0084\"\
   },\
   {\
      \"code\" : \"G0085\",\
      \"description\" : \"Hexane\"\
   },\
   {\
      \"code\" : \"G0086\",\
      \"description\" : \"Hydrazine\"\
   },\
   {\
      \"code\" : \"G0087\",\
      \"description\" : \"Isoamyl acetate\"\
   },\
   {\
      \"description\" : \"Isopropylamine\",\
      \"code\" : \"G0088\"\
   },\
   {\
      \"description\" : \"Isopropyl ether\",\
      \"code\" : \"G0089\"\
   },\
   {\
      \"code\" : \"G0090\",\
      \"description\" : \"Isobutanol\"\
   },\
   {\
      \"description\" : \"Isobutylene\",\
      \"code\" : \"G0091\"\
   },\
   {\
      \"code\" : \"G0092\",\
      \"description\" : \"Isooctane\"\
   },\
   {\
      \"code\" : \"G0093\",\
      \"description\" : \"Isophorone\"\
   },\
   {\
      \"description\" : \"Isopropanol\",\
      \"code\" : \"G0094\"\
   },\
   {\
      \"code\" : \"G0095\",\
      \"description\" : \"Jet A Fuel\"\
   },\
   {\
      \"code\" : \"G0096\",\
      \"description\" : \"Jet A1 Fuel\"\
   },\
   {\
      \"code\" : \"G0097\",\
      \"description\" : \"JP-5 & JP-8 Fuel\"\
   },\
   {\
      \"code\" : \"G0098\",\
      \"description\" : \"MEK (methylethyl ketone)\"\
   },\
   {\
      \"code\" : \"G0099\",\
      \"description\" : \"Mesityl oxide\"\
   },\
   {\
      \"description\" : \"MIBK (methyl-isobutyl ketone\",\
      \"code\" : \"G0100\"\
   },\
   {\
      \"code\" : \"G0101\",\
      \"description\" : \"Monomethylamine\"\
   },\
   {\
      \"description\" : \"MTBE (Methyl-tertbutyl ether)\",\
      \"code\" : \"G0102\"\
   },\
   {\
      \"code\" : \"G0103\",\
      \"description\" : \"Methylbenzyl alcohol\"\
   },\
   {\
      \"code\" : \"G0104\",\
      \"description\" : \"m-Xylene\"\
   },\
   {\
      \"description\" : \"n-Methylpyrrolidone\",\
      \"code\" : \"G0105\"\
   },\
   {\
      \"description\" : \"Octane\",\
      \"code\" : \"G0106\"\
   },\
   {\
      \"code\" : \"G0107\",\
      \"description\" : \"o-Xylene\"\
   },\
   {\
      \"code\" : \"G0108\",\
      \"description\" : \"Phenelethyl alcohol\"\
   },\
   {\
      \"code\" : \"G0109\",\
      \"description\" : \"Phenol\"\
   },\
   {\
      \"code\" : \"G0110\",\
      \"description\" : \"Phosphine\"\
   },\
   {\
      \"description\" : \"Propylene\",\
      \"code\" : \"G0111\"\
   },\
   {\
      \"code\" : \"G0112\",\
      \"description\" : \"Propylene oxide\"\
   },\
   {\
      \"description\" : \"p-Xylene\",\
      \"code\" : \"G0113\"\
   },\
   {\
      \"code\" : \"G0114\",\
      \"description\" : \"Pryridine\"\
   },\
   {\
      \"code\" : \"G0115\",\
      \"description\" : \"Quinoline\"\
   },\
   {\
      \"description\" : \"Styrene\",\
      \"code\" : \"G0116\"\
   },\
   {\
      \"description\" : \"tert-Butylamine\",\
      \"code\" : \"G0117\"\
   },\
   {\
      \"code\" : \"G0118\",\
      \"description\" : \"Dichloromethane\"\
   },\
   {\
      \"code\" : \"G0119\",\
      \"description\" : \"tert-Butyl mercaptan\"\
   },\
   {\
      \"description\" : \"tert-Butyl alcohol\",\
      \"code\" : \"G0120\"\
   },\
   {\
      \"description\" : \"THF (Tetrahydrofuran)\",\
      \"code\" : \"G0121\"\
   },\
   {\
      \"description\" : \"Thiophene\",\
      \"code\" : \"G0122\"\
   },\
   {\
      \"code\" : \"G0123\",\
      \"description\" : \"Toluene\"\
   },\
   {\
      \"code\" : \"G0124\",\
      \"description\" : \"Turpentine\"\
   },\
   {\
      \"description\" : \"Vinylcylclohexene\",\
      \"code\" : \"G0125\"\
   },\
   {\
      \"code\" : \"G0126\",\
      \"description\" : \"Vinyl acetate\"\
   },\
   {\
      \"code\" : \"G0127\",\
      \"description\" : \"Vinyl chloride\"\
   },\
   {\
      \"description\" : \"Benzene Tube\",\
      \"code\" : \"G0128\"\
   },\
   {\
      \"description\" : \"Nitrogen\",\
      \"code\" : \"G0130\"\
   },\
   {\
      \"code\" : \"G0132\",\
      \"description\" : \"Butadiene\"\
   },\
   {\
      \"description\" : \"Fresh Air\",\
      \"code\" : \"G9999\"\
   },\
   {\
      \"description\" : \"Unidentified Combustible Gas\",\
      \"code\" : \"G0019\"\
   },\
   {\
      \"code\" : \"G0061\",\
      \"description\" : \"Butadiene\"\
   },\
   {\
      \"description\" : \"Acetylene\",\
      \"code\" : \"G0200\"\
   },\
   {\
      \"code\" : \"G0201\",\
      \"description\" : \"Butane\"\
   },\
   {\
      \"description\" : \"Ethane\",\
      \"code\" : \"G0202\"\
   },\
   {\
      \"description\" : \"Methanol\",\
      \"code\" : \"G0203\"\
   },\
   {\
      \"code\" : \"G0205\",\
      \"description\" : \"JP-4 fuel\"\
   },\
   {\
      \"description\" : \"JP-5 fuel\",\
      \"code\" : \"G0206\"\
   },\
   {\
      \"description\" : \"JP-8 fuel\",\
      \"code\" : \"G0207\"\
   },\
   {\
      \"code\" : \"G0208\",\
      \"description\" : \"Acetic acid\"\
   },\
   {\
      \"code\" : \"G0209\",\
      \"description\" : \"Acetic Anhydride\"\
   },\
   {\
      \"description\" : \"Arsine\",\
      \"code\" : \"G0210\"\
   },\
   {\
      \"code\" : \"G0211\",\
      \"description\" : \"Bromine\"\
   },\
   {\
      \"description\" : \"Carbon disulfide\",\
      \"code\" : \"G0212\"\
   },\
   {\
      \"code\" : \"G0213\",\
      \"description\" : \"Cyclohexene\"\
   },\
   {\
      \"code\" : \"G0214\",\
      \"description\" : \"Diesel fuel\"\
   },\
   {\
      \"code\" : \"G0215\",\
      \"description\" : \"Dimethyl sulfoxide\"\
   },\
   {\
      \"description\" : \"Ethyl ether\",\
      \"code\" : \"G0216\"\
   },\
   {\
      \"code\" : \"G0217\",\
      \"description\" : \"Iodine\"\
   },\
   {\
      \"description\" : \"Methyl mercaptan\",\
      \"code\" : \"G0218\"\
   },\
   {\
      \"code\" : \"G0219\",\
      \"description\" : \"Naphthalene\"\
   },\
   {\
      \"description\" : \"Nitrobenzene\",\
      \"code\" : \"G0220\"\
   },\
   {\
      \"description\" : \"Methoxyethoxyethanol,2-\",\
      \"code\" : \"G0221\"\
   },\
   {\
      \"description\" : \"Photo Ionization Detector\",\
      \"code\" : \"G0250\"\
   },\
   {\
      \"code\" : \"G0251\",\
      \"description\" : \"Photo Ionization Detector\"\
   },\
   {\
      \"code\" : \"G0252\",\
      \"description\" : \"Photo Ionization Detector\"\
   },\
   {\
      \"code\" : \"G0253\",\
      \"description\" : \"Photo Ionization Detector\"\
   },\
   {\
      \"code\" : \"G0254\",\
      \"description\" : \"Photo Ionization Detector\"\
   },\
   {\
      \"description\" : \"Isobutane\",\
      \"code\" : \"G0133\"\
   },\
   {\
      \"description\" : \"Nonane\",\
      \"code\" : \"G0222\"\
   },\
   {\
      \"code\" : \"G0248\",\
      \"description\" : \"Hydrocarbon\"\
   },\
   {\
      \"description\" : \"Panic Alarm\",\
      \"code\" : \"G0247\"\
   },\
   {\
      \"code\" : \"G0246\",\
      \"description\" : \"Man Down Alarm\"\
   },\
   {\
      \"code\" : \"G0245\",\
      \"description\" : \"Proximity Alarm\"\
   }\
]";
