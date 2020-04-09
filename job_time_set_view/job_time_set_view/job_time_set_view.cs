using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.Xml;

namespace job_time_set_view
{
    public partial class job_time_set_view : Form
    {
        public job_time_set_view()
        {
            InitializeComponent();
        }

        private void job_time_set_view_on_drag_drop(object sender, System.Windows.Forms.DragEventArgs e)
        {
            var dropped = e.Data.GetData(DataFormats.FileDrop);
            if (dropped.GetType() != typeof(System.String[]))
            {
                e.Effect = DragDropEffects.None;
                return;
            }
            string[] filesDropped = dropped as string[];
            string first = filesDropped[0];

            if (!first.Contains("job_time_sets.xml"))
            {
                e.Effect = DragDropEffects.None;
                return;
            }

            XmlDocument newDoc = new XmlDocument();
            newDoc.LoadXml(first);
        }
        private void job_time_set_view_drag_enter(object sender, System.Windows.Forms.DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
                e.Effect = DragDropEffects.All;
            else
                e.Effect = DragDropEffects.None;
        }
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {

        }

        private void job_time_set_view_Load(object sender, EventArgs e)
        {

        }

        private void textBox1_TextChanged(object sender, EventArgs e)
        {

        }

        private void label1_Click(object sender, EventArgs e)
        {

        }

        private void tabPage1_Click(object sender, EventArgs e)
        {

        }
        private void tabPage2_Click(object sender, EventArgs e)
        {

        }
        private void tabPage3_Click(object sender, EventArgs e)
        {

        }

        private void listBox1_SelectedIndexChanged_1(object sender, EventArgs e)
        {

        }

        private void label2_Click(object sender, EventArgs e)
        {

        }

        private void chart1_Click(object sender, EventArgs e)
        {

        }
    }
}
